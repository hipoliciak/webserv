# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: webserv <webserv@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/01/01 00:00:00 by webserv          #+#    #+#              #
#    Updated: 2025/01/01 00:00:00 by webserv         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = webserv

# Compiler and flags
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
DEBUG_FLAGS = -g -fsanitize=address
RELEASE_FLAGS = -O2 -DNDEBUG

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
CONFIGDIR = config
WWWDIR = www

# Source files
SOURCES = main.cpp \
          Server.cpp \
          Client.cpp \
          HttpRequest.cpp \
          HttpResponse.cpp \
          Config.cpp \
          CGI.cpp

# Object files
OBJECTS = $(SOURCES:%.cpp=$(OBJDIR)/%.o)

# Dependencies
DEPS = $(OBJECTS:.o=.d)

# Include paths
INCLUDES = -I$(INCDIR)

# Colors for output
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[0;33m
BLUE = \033[0;34m
MAGENTA = \033[0;35m
CYAN = \033[0;36m
NC = \033[0m # No Color

# Default target
all: $(NAME)

# Debug build
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(NAME)

# Release build
release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(NAME)

# Main target
$(NAME): $(OBJECTS)
	@echo "$(CYAN)Linking $(NAME)...$(NC)"
	@$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(NAME)
	@echo "$(GREEN)✓ $(NAME) created successfully!$(NC)"

# Object files compilation
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Include dependency files
-include $(DEPS)

# Clean object files and dependencies
clean:
	@echo "$(RED)Cleaning object files...$(NC)"
	@rm -rf $(OBJDIR)

# Clean everything
fclean: clean
	@echo "$(RED)Cleaning $(NAME)...$(NC)"
	@rm -f $(NAME)

# Rebuild everything
re: fclean all

# Setup project structure (create missing directories and files)
setup:
	@echo "$(BLUE)Setting up project structure...$(NC)"
	@mkdir -p $(WWWDIR)/error
	@mkdir -p $(WWWDIR)/images
	@mkdir -p $(WWWDIR)/css
	@mkdir -p $(WWWDIR)/js
	@mkdir -p tests
	@echo "$(GREEN)✓ Project structure created!$(NC)"

# Run the server with default config
run: $(NAME)
	@echo "$(CYAN)Starting webserv...$(NC)"
	@./$(NAME)

# Run with custom config
run-config: $(NAME)
	@echo "$(CYAN)Starting webserv with config/production.conf...$(NC)"
	@./$(NAME) config/production.conf

# Test the server
test: $(NAME)
	@echo "$(YELLOW)Testing webserv...$(NC)"
	@echo "$(BLUE)Starting server in background...$(NC)"
	@./$(NAME) &
	@echo $$! > webserv.pid
	@sleep 2
	@echo "$(YELLOW)Testing GET request...$(NC)"
	@curl -s http://localhost:8080/ > /dev/null && echo "$(GREEN)✓ GET / works$(NC)" || echo "$(RED)✗ GET / failed$(NC)"
	@echo "$(YELLOW)Testing 404 error...$(NC)"
	@curl -s http://localhost:8080/nonexistent > /dev/null && echo "$(RED)✗ 404 test failed$(NC)" || echo "$(GREEN)✓ 404 handling works$(NC)"
	@echo "$(BLUE)Stopping server...$(NC)"
	@kill `cat webserv.pid` 2>/dev/null || true
	@rm -f webserv.pid

# Install dependencies (if any)
install:
	@echo "$(BLUE)No dependencies to install for this project.$(NC)"

# Show help
help:
	@echo "$(CYAN)Available targets:$(NC)"
	@echo "  $(GREEN)all$(NC)         - Build the project"
	@echo "  $(GREEN)debug$(NC)       - Build with debug flags"
	@echo "  $(GREEN)release$(NC)     - Build with optimization flags"
	@echo "  $(GREEN)clean$(NC)       - Remove object files"
	@echo "  $(GREEN)fclean$(NC)      - Remove all generated files"
	@echo "  $(GREEN)re$(NC)          - Rebuild everything"
	@echo "  $(GREEN)setup$(NC)       - Create project structure"
	@echo "  $(GREEN)run$(NC)         - Run webserv with default config"
	@echo "  $(GREEN)run-config$(NC)  - Run webserv with production config"
	@echo "  $(GREEN)test$(NC)        - Test the server functionality"
	@echo "  $(GREEN)install$(NC)     - Install dependencies"
	@echo "  $(GREEN)help$(NC)        - Show this help message"

# Declare phony targets
.PHONY: all debug release clean fclean re setup run run-config test install help

# Prevent deletion of intermediate files
.PRECIOUS: $(OBJDIR)/%.o