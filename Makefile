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
	@echo "✓ $(NAME) created successfully!"

# Object files compilation
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Include dependency files
-include $(DEPS)

# Clean object files
clean:
	@rm -rf $(OBJDIR)

# Clean everything
fclean: clean
	@rm -f $(NAME)

# Rebuild everything
re: fclean all

# Run the server
run: $(NAME)
	@./$(NAME)

# Declare phony targets
.PHONY: all clean fclean re run