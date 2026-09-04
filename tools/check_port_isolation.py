#!/usr/bin/env python3
"""
Port Isolation Checker for Nexos-RT V2
Verifies that core microkernel components (core, ipc, memory, time, include)
do NOT include any FreeRTOS headers or vendor scheduler headers directly.
Only files in port/ and arch/ may touch platform-specific headers.
"""

import os
import sys
import re

FORBIDDEN_PATTERNS = [
    re.compile(r'#include\s+[<"]freertos/'),
    re.compile(r'#include\s+[<"]rom/'),
]

ALLOWED_DIRS = {
    os.path.normpath("components/microkernel/port"),
    os.path.normpath("components/microkernel/arch"),
}

CORE_DIRS = [
    os.path.normpath("components/microkernel/core"),
    os.path.normpath("components/microkernel/ipc"),
    os.path.normpath("components/microkernel/memory"),
    os.path.normpath("components/microkernel/time"),
    os.path.normpath("components/microkernel/include"),
]

def check_isolation(workspace_root):
    violations = []
    
    for core_dir in CORE_DIRS:
        full_path = os.path.join(workspace_root, core_dir)
        if not os.path.exists(full_path):
            continue
            
        for root, _, files in os.walk(full_path):
            for f in files:
                if f.endswith(('.c', '.h', '.cpp', '.hpp')):
                    file_path = os.path.join(root, f)
                    rel_path = os.path.relpath(file_path, workspace_root)
                    
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as fp:
                        for line_no, line in enumerate(fp, start=1):
                            for pattern in FORBIDDEN_PATTERNS:
                                if pattern.search(line):
                                    violations.append((rel_path, line_no, line.strip()))
                                    
    return violations

def main():
    workspace = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    print(f"Checking Nexos-RT V2 port isolation in: {workspace}")
    violations = check_isolation(workspace)
    
    if violations:
        print("\n[FAIL] Port isolation violations detected in kernel core:")
        for path, line_no, line in violations:
            print(f"  {path}:{line_no} -> {line}")
        sys.exit(1)
    else:
        print("\n[PASS] Kernel core is 100% isolated from vendor FreeRTOS headers!")
        sys.exit(0)

if __name__ == "__main__":
    main()
