lshl: A simple Windows command-line program that lists files in a given directory that have multiple hardlinks.

It uses the Windows file management API to identify files that have multiple hardlinks.

Building:
Using Microsoft's C++ compiler: cl /EHsc lshl.cpp

Usage:
lshl [Absolute Path to Directory]

Example:
lshl C:\Windows\winsxs (This lists all files with hardlinks in the specified directory and all it's subdirectories.)
lshl (This lists all files with hardlinks in the current working directory and all it's subdirectories.)
