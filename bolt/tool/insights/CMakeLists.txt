# Copyright (c) ByteDance Ltd. and/or its affiliates.
# SPDX-License-Identifier: Apache-2.0

add_library(
  bolt_tool_insights
  PolicyAgent.cpp
  InsightDemoRunner.cpp
)

target_link_libraries(
  bolt_tool_insights
  PUBLIC
  bolt_exec_insights
  bolt_exec
  bolt_common_base
  bolt_exec_test_lib
  bolt_vector_test_lib
  bolt_type
  bolt_type_tz
)

add_executable(
  bolt_insights_tool
  InsightsMain.cpp
)

target_link_libraries(
  bolt_insights_tool
  PRIVATE
  bolt_tool_insights
  bolt_engine
  bolt_exec
  bolt_exec_test_lib
  bolt_vector_test_lib
  bolt_vector_fuzzer
  bolt_temp_path
  bolt_core
  bolt_exception
  bolt_expression
  bolt_parse_parser
  bolt_parse_expression
  bolt_duckdb_parser
  bolt_duckdb_conversion
  bolt_dwio_common
  bolt_dwio_dwrf_reader
  bolt_dwio_dwrf_writer
  bolt_dwio_common_test_utils
  bolt_file_test_utils
  bolt_type_fbhive
  bolt_hive_connector
  bolt_tpch_connector
  bolt_tpch_gen
  bolt_dbgen
  bolt_presto_serializer
  bolt_functions_prestosql
  bolt_aggregates
  gflags
  glog::glog
)

if(BOLT_BUILD_TESTING)
  add_subdirectory(tests)
endif()
