SOURCES = fw_tool.c \
		  user_config.c \
		  image_data.h \
		  include/default_config.h 

rev_fw_tool_SOURCES = rev_fw_tool.c \
			  		  include/default_config.h

old_config_convert_SOURCES = old_config_convert.c \
							 user_config.c \
							 image_data.h \
							 include/user_config.h


CFLAGS = -g -O0 -Wall -DFWTOOLS
INCLUDE =	 -I/usr/include \
		 -I./include \
		 -I../../include

all: fw_tool rev_fw_tool

fw_tool: $(SOURCES) 
	gcc $(CFLAGS) $(INCLUDE) $(SOURCES) -o fw_tool 

rev_fw_tool: $(rev_fw_tool_SOURCES)
	gcc $(CFLAGS) $(INCLUDE) $(rev_fw_tool_SOURCES) -o rev_fw_tool

# config_convert should be removed once all old style validation
# user_config.c files have been converted to newer binary blobs.
old_config_convert: $(old_config_convert_SOURCES)
	gcc $(CFLAGS) $(old_config_convert_SOURCES) -o old_config_convert

clean:
	rm -f fw_tool rev_fw_tool old_config_convert
