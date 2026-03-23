#!/bin/bash
sed -i 's/std::atomic<bool> closed_{false};/std::atomic<bool> closed_{false};\n  uint64_t nextSequenceId_{0};/' bolt/exec/insights/TaskInsightSession.h
