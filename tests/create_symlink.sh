#!/bin/bash
# Script to create test directory with symlinks for testing copy/move operations
# Usage: ./create_symlink_testdata.sh [target_dir]
# Default target: /tmp/test_symlinks

TEST_DIR="${1:-/tmp/test_symlinks}"

# Clean up and create fresh
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

# Create regular files
echo "file1 content" > file1.txt
echo "file2 content" > file2.txt
echo "file3 content" > file3.txt

# Create symlink to file
ln -s file1.txt link_do_file1.txt

# Create subdirectory with file
mkdir -p subdir
echo "file in subdir" > subdir/file_inside.txt

# Create symlink to directory
ln -s subdir link_to_subdir

# Show result
echo "Created test directory: $TEST_DIR"
echo ""
ls -la "$TEST_DIR"
echo ""
echo "Subdirectory contents:"
ls -la "$TEST_DIR/subdir"
echo ""
echo "Test scenarios:"
echo "1. Same FS copy (F5): symlinks should be copied as symlinks"
echo "2. Same FS move (F6): symlinks should be moved (renamed)"
echo "3. Cross-FS copy/move: symlinks should be skipped with info message"
