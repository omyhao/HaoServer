include config.mk
all:
	@for dir in $(Build_Dir); \
	do \
		make -C $$dir; \
	done

clean:
	rm -rf app/link_obj app/dep hao_server