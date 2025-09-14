#include "../include/webserv.hpp"
#include "../include/Server.hpp"
#include "../include/Config.hpp"

// Global variables for signal handling
static bool g_serverRunning = true;
static Server* g_server = NULL;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nShutting down server..." << std::endl;
        g_serverRunning = false;
        if (g_server) {
            g_server->stop();
        }
    }
}

int main(int argc, char **argv) {
    try {
        std::string configFile = "config/default.conf";
        
        // Parse command line arguments
        if (argc > 1) {
            configFile = argv[1];
        }
        
        std::cout << "Starting webserv..." << std::endl;
        std::cout << "Config file: " << configFile << std::endl;
        
        // Load configuration
        Config config(configFile);
        if (!config.parse()) {
            std::cerr << "Error: Failed to parse configuration file" << std::endl;
            return 1;
        }
        
        // Create and initialize server
        Server server(config);
        g_server = &server;  // Set global pointer for signal handler
        
        if (!server.initialize()) {
            std::cerr << "Error: Failed to initialize server" << std::endl;
            return 1;
        }
        
        std::cout << "Server listening on " << server.getHost() << ":" << server.getPort() << std::endl;
        
        // Handle SIGINT for graceful shutdown
        signal(SIGINT, signalHandler);
        
        // Run the server
        server.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}