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

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Source files
SOURCES = main.cpp \
          Server.cpp \
          Client.cpp \
          HttpRequest.cpp \
          HttpResponse.cpp \
          Config.cpp \
          CGI.cpp \
          Utils.cpp

# Colors for output
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[0;33m
BLUE = \033[0;34m
MAGENTA = \033[0;35m
CYAN = \033[0;36m
NC = \033[0m # No Color

# Object files
OBJECTS = $(SOURCES:%.cpp=$(OBJDIR)/%.o)

# Dependencies
DEPS = $(OBJECTS:.o=.d)

# Include paths
INCLUDES = -I$(INCDIR)

# Default target
all: $(NAME)

# Main target
$(NAME): $(OBJECTS)
	@$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(NAME)
	@echo "$(GREEN)✓ $(NAME) created successfully!$(NC)"

# Object files compilation
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Include dependency files
-include $(DEPS)

# Clean object files
clean:
	@echo "$(RED)Cleaning object files...$(NC)"
	@rm -rf $(OBJDIR)

# Clean everything
fclean: clean
	@echo "$(RED)Cleaning $(NAME)...$(NC)"
	@rm -f $(NAME)

# Rebuild everything
re: fclean all

# Run the server
run: $(NAME)
	@./$(NAME)

# Declare phony targets
.PHONY: all clean fclean re run