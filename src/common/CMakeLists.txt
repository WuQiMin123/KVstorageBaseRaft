#获取当前文件所在目录的绝对路径
get_filename_component(SRC_DIR, "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

#将该目录下所有源文件添加到 SRC_LIST 变量中
aux_source_directory($(SRC_DIR) SRC_LIST)

#将这些源文件路径保存在一个内部缓存变量 src_common 中，以便后续用于构建目标
set(src_common, ${SRC_LIST} CACHE INTERNAL "Description of the variable")