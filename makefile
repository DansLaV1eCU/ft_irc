NAME := ircserv
OBJDIR := obj

CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98 -Iincludes

SRC := src/main.cpp src/Client.cpp src/Channel.cpp src/Server.cpp
OBJ := $(SRC:%.cpp=$(OBJDIR)/%.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

define build_object
$(1): $(2) includes/Server.hpp includes/Client.hpp includes/Channel.hpp
	mkdir -p $(dir $(1))
	$(CXX) $(CXXFLAGS) -c $(2) -o $(1)
endef

$(foreach source,$(SRC),$(eval $(call build_object,$(patsubst %.cpp,$(OBJDIR)/%.o,$(source)),$(source))))

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
