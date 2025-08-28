#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <inotifytools/inotify.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/select.h>
#include <fcntl.h>

namespace fs = std::filesystem;

// Configuration specific to the Framework 12
const std::string TOGGLE_FILE = std::string(std::getenv("HOME")) + "/.config/hypr/rotation-toggle";
const std::string MONITOR_NAME = "eDP-1";
const std::string RESOLUTION = "1920x1200@60";
const std::string SCALE = "1.2"; //my particular scaling preference

// Function to execute hyprctl transform commands
void setOrientation(int transform) {
    std::string monitor_cmd = "hyprctl keyword monitor \"" + MONITOR_NAME + "," +
                             RESOLUTION + ",0x0," + SCALE + ",transform," +
                             std::to_string(transform) + "\"";
    std::string touch_cmd = "hyprctl keyword input:touchdevice:transform " +
                            std::to_string(transform);
    std::string tablet_cmd = "hyprctl keyword input:tablet:transform " +
                             std::to_string(transform);

    //std::cerr << "Applying orientation: transform=" << transform << std::endl;
    system(monitor_cmd.c_str());
    system(touch_cmd.c_str());
    system(tablet_cmd.c_str());
}

// Function to read toggle state
bool readToggleState() {
    std::ifstream file(TOGGLE_FILE);
    int state = 0;
    if (file.is_open()) {
        file >> state;
        file.close();
    }
    return state;
}

// Function to ensure toggle file exists
void ensureToggleFile() {
    // if rotation-toggle doesn't exist, create it and set rotation to enabled
    if (!fs::exists(TOGGLE_FILE)) {
        fs::create_directories(fs::path(TOGGLE_FILE).parent_path());
        std::ofstream file(TOGGLE_FILE);
        file << "1";
        file.close();
    }
}

// Function to get current orientation from iio-sensor-proxy via D-Bus
std::string getCurrentOrientation() {
    FILE* p = popen("dbus-send --system --print-reply --dest=net.hadess.SensorProxy /net/hadess/SensorProxy org.freedesktop.DBus.Properties.Get string:\"net.hadess.SensorProxy\" string:\"AccelerometerOrientation\"", "r");
    if (!p) {
        std::cerr << "Failed to query current orientation" << std::endl;
        return "";
    }

    char buf[512] = {0};
    while (fgets(buf + strlen(buf), sizeof(buf) - strlen(buf), p) != nullptr) {}
    pclose(p);

    std::string output(buf);
    size_t variant_pos = output.find("variant");
    if (variant_pos == std::string::npos) return "";

    size_t quote_pos = output.find("\"", variant_pos);
    if (quote_pos == std::string::npos) return "";

    size_t end_quote_pos = output.find("\"", quote_pos + 1);
    if (end_quote_pos == std::string::npos) return "";

    return output.substr(quote_pos + 1, end_quote_pos - quote_pos - 1);
}

int main() {
    // Ensure toggle file exists
    ensureToggleFile();

    // Initialize inotify
    int fd = inotify_init();
    if (fd < 0) {
        std::cerr << "Failed to initialize inotify: " << std::strerror(errno) << std::endl;
        return 1;
    }

    // Add watch for toggle file
    int wd = inotify_add_watch(fd, TOGGLE_FILE.c_str(), IN_MODIFY);
    if (wd < 0) {
        std::cerr << "Failed to add inotify watch for " << TOGGLE_FILE << ": " << std::strerror(errno) << std::endl;
        close(fd);
        return 1;
    }

    // Start monitor-sensor
    FILE* pipe = popen("monitor-sensor", "r");
    if (!pipe) {
        std::cerr << "Failed to start monitor-sensor: " << std::strerror(errno) << std::endl;
        inotify_rm_watch(fd, wd);
        close(fd);
        return 1;
    }

    int pipe_fd = fileno(pipe);
    // Set pipe to non-blocking
    int flags = fcntl(pipe_fd, F_GETFL, 0);
    fcntl(pipe_fd, F_SETFL, flags | O_NONBLOCK);

    // Main loop
    bool rotation_enabled = readToggleState();
    std::string last_orientation = "";
    char line[256];

    while (true) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set); // Always monitor inotify
        int max_fd = fd;

        if (rotation_enabled) {
            FD_SET(pipe_fd, &set);
            max_fd = std::max(max_fd, pipe_fd);
        }

        struct timeval timeout = {0, 100000}; // 100ms timeout
        int ret = select(max_fd + 1, &set, nullptr, nullptr, &timeout);
        if (ret < 0) {
            //std::cerr << "select error: " << std::strerror(errno) << std::endl;
            continue;
        }

        if (FD_ISSET(fd, &set)) {
            // Read inotify event (consume it)
            char event_buf[512];
            read(fd, event_buf, sizeof(event_buf));

            bool new_state = readToggleState();
            if (new_state != rotation_enabled) {
                rotation_enabled = new_state;
                //std::cerr << "Toggle state changed: rotation_enabled=" << rotation_enabled << std::endl;
                if (rotation_enabled) {
                    // Drain any stale data from pipe
                    while (fgets(line, sizeof(line), pipe) != nullptr) {
                        // Discard
                    }

                    // Apply current orientation
                    std::string current = getCurrentOrientation();
                    if (!current.empty() && current != "undefined" && current != last_orientation) {
                        int transform = -1;
                        if (current == "normal") transform = 0;
                        else if (current == "right-up") transform = 3;
                        else if (current == "left-up") transform = 1;
                        else if (current == "bottom-up") transform = 2;
                        if (transform != -1) {
                            setOrientation(transform);
                            last_orientation = current;
                        }
                    }
                } else {
                    // Reset last orientation when disabling
                    last_orientation = "";
                }
            }
        }

        if (rotation_enabled && FD_ISSET(pipe_fd, &set)) {
            // Read all available lines from monitor-sensor
            while (fgets(line, sizeof(line), pipe) != nullptr) {
                std::string orientation(line);
                // Remove trailing newline
                if (!orientation.empty() && orientation.back() == '\n') {
                    orientation.pop_back();
                }

                // Debug output
                //std::cerr << "Orientation: " << orientation << std::endl;

                // Extract the orientation part after "changed: "
                size_t changed_pos = orientation.find("changed: ");
                if (changed_pos != std::string::npos) {
                    orientation = orientation.substr(changed_pos + 9);
                }

                // Parse orientation, but only apply if different from last
                if (orientation != last_orientation) {
                    if (orientation.find("normal") != std::string::npos) {
                        setOrientation(0);
                        last_orientation = orientation;
                    } else if (orientation.find("right-up") != std::string::npos) {
                        setOrientation(3);
                        last_orientation = orientation;
                    } else if (orientation.find("left-up") != std::string::npos) {
                        setOrientation(1);
                        last_orientation = orientation;
                    } else if (orientation.find("bottom-up") != std::string::npos) {
                        setOrientation(2);
                        last_orientation = orientation;
                    }
                }
            }
        }

        //std::cerr << "Rotation logic processed. Rotation enabled: " << rotation_enabled << std::endl;
    }

    // Cleanup
    pclose(pipe);
    inotify_rm_watch(fd, wd);
    close(fd);
    return 0;
}