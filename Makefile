NAME		=	webserv

SRCS_DIR	=	srcs/
OBJ_DIR		=	build/

INCLUDES	=	-I $(SRCS_DIR)

CXX			=	c++
CXXFLAGS	=	-Wall -Wextra -Werror -std=c++98 -g3 -MMD -MP $(INCLUDES)

RM			=	rm -rf

SRCS		=	main.cpp					\
				config/Config.cpp			\
				log/Log.cpp					\
				utils/Utils.cpp				\
				utils/Utils_str.cpp			\
				event/Reactor.cpp			\
				signal/signal.cpp			\
				server/Server.cpp			\
				connection/Connection.cpp	\
				request/RequestParser.cpp	\
				response/ResponseBuilder.cpp	\
				response/ResponseBuilder_cgi.cpp	\
				cgi/CgiHandler.cpp			\
				cgi/CgiProcess.cpp

OBJS		=	$(addprefix $(OBJ_DIR), $(SRCS:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(OBJ_DIR)%.o: $(SRCS_DIR)%.cpp Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
