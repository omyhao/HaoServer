MAKEFLAGS += --no-print-directory
export Build_Root = $(shell pwd)
export Include_Path = $(Build_Root)/_include


# 先写子目录，最后写根目录
Build_Dir = $(Build_Root)/app/log/ \
			$(Build_Root)/app/signal/ \
			$(Build_Root)/app/proc/ \
			$(Build_Root)/app/net/ \
			$(Build_Root)/app/util/ \
			$(Build_Root)/app/logic/ \
			$(Build_Root)/app/



# 调试工具，包括valgrind来使用
export DEBUG = true