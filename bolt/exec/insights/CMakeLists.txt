bolt_add_library(
  bolt_exec_insights
  InsightEventJson.cpp
)

target_link_libraries(
  bolt_exec_insights
  bolt_exec
)

if(${BOLT_BUILD_TESTING})
  add_subdirectory(tests)
endif()
