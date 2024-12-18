#include "drone.h"
#include <signal.h>
#include <atomic>

static DroneFleet* g_drone_fleet = nullptr;
static std::atomic<bool> shutdown_in_progress(false);
static std::atomic<bool> data_saved(false);

void signal_handler(int signum) {
    // Ensure handler only runs once
    bool expected = false;
    if (!shutdown_in_progress.compare_exchange_strong(expected, true)) {
        printf("\nShutdown already in progress...\n");
        fflush(stdout);
        return;
    }

    printf("\n\n====================================\n");
    printf("Process interrupted by signal %d\n", signum);
    printf("Attempting graceful shutdown...\n");
    fflush(stdout);

    if (g_drone_fleet && !data_saved) {
        try {
            printf("Saving current simulation state...\n");
            fflush(stdout);

            g_drone_fleet->log();  // Save the current state
            data_saved = true;

            printf("Data saved successfully to Redis streams\n");
            fflush(stdout);

            g_drone_fleet = nullptr;  // Prevent further access

        } catch (const std::exception& e) {
            printf("\nError during shutdown: %s\n", e.what());
            fflush(stdout);
        }
    } else if (data_saved) {
        printf("Data already saved\n");
    } else {
        printf("Warning: No data to save - simulation hadn't started.\n");
    }

    printf("====================================\n");
    fflush(stdout);

    // Small delay to ensure messages are printed
    usleep(100000);  // 100ms delay

    _Exit(signum);  // Use _Exit instead of exit for immediate termination
}

int main() {
    try {
        // Register signal handlers
        signal(SIGINT, signal_handler);   // Ctrl+C
        signal(SIGTERM, signal_handler);  // Termination request
        signal(SIGABRT, signal_handler);  // Abort signal

        // Initialize drone fleet
        DroneFleet fleet = DroneFleet();
        g_drone_fleet = &fleet;

        printf("\nInitializing drone fleet simulation...\n");
        fleet.init();
        fleet.init_drones(DRONES_COUNT);
        printf("Initialized %d drones successfully\n\n", DRONES_COUNT);

        // Main simulation loop
        for (int time_step = 0; time_step < HORIZON; time_step++) {
            if (shutdown_in_progress) {
                break;  // Exit loop if shutdown started
            }

            printf("\n=========================\n");
            printf("Simulation Time: %d\n", time_step);
            printf("=========================\n\n");

            fleet.sync_with_control_center();
            fleet.update_position(time_step);
            fleet.log();
        }

        // Normal cleanup
        if (!shutdown_in_progress) {
            printf("\nSimulation completed successfully.\n");
            printf("All data has been saved to Redis streams.\n");
            fleet.shutdown();
            printf("Redis connections closed.\n");
        }
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        fprintf(stderr, "\nError: %s\n", e.what());
        if (g_drone_fleet && !data_saved && !shutdown_in_progress) {
            printf("Attempting to save data before exit...\n");
            g_drone_fleet->log();
            g_drone_fleet->shutdown();
            printf("Emergency data save completed. Check Redis streams for the latest state.\n");
        }
        return EXIT_FAILURE;
    }
}