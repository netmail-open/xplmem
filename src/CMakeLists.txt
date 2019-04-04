add_library(xplmem SHARED
	memmgr.c
)

target_link_libraries(xplmem
  xpllock
  xplthread
)


install(TARGETS xplmem DESTINATION /usr/lib)

