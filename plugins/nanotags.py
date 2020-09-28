#!/usr/bin/python3
import time
import hashlib
import tempfile
import _thread
import pynano
import ctags
#from ctags import CTags, TagEntry
from binascii import hexlify
import os
from pprint import pprint
from clang.cindex import *
import re
import sys
import atexit

defs_pattern = re.compile("\-D(?P<define>[A-Za-z0-9_]+)")

builddefs = []
files = {}
cursors = {}
sources = {}

decltypes = [ getattr(CursorKind, x) for x in filter(lambda x: "_DECL" in x, dir(CursorKind)) ]
declarations = {}

def _find_real_funcdef(curfile, symbol):
	global cursors
	subdir = os.path.dirname(curfile)
	#pynano.output_text("%d" % len(files))

	for f in files:
		#if not f.startswith(os.path.relpath(subdir)):
		if subdir + os.path.sep not in f:
			continue

		cursors[f] = parse_source_file(f, builddefs)
		map_source_file(f)

		# Don't have to parse everything...
		if declarations[symbol.spelling].is_definition():
			return f

def sha256sum(data):
	h = hashlib.sha256()
	h.update(bytes(str(data), "ascii"))
	return hexlify(h.digest()).decode()

def _reparse(filename):
	if not pynano.is_modified(filename):
		return

	content = pynano.get_buffer_content(filename)

	hash = sha256sum(content)

	filename = _get_file(filename, files)

	if hash == files[filename]:
		return

	files[filename] = hash

	fname, ext = os.path.splitext(filename)

	with tempfile.NamedTemporaryFile(mode = "w", suffix = ext) as f:
		f.write(content)
		f.flush()
		cursors[filename] = parse_source_file(f.name, builddefs)
		map_source_file(filename)
		pynano.output_text("Refreshed")

def goto_definition():
	global cursors
	global sources

	curfile = pynano.get_current_file()

	line, col = pynano.get_current_position()

	#pynano.output_text("Is this modified? " + str(pynano.is_modified(curfile)))

	# Call reparse just in case
	_reparse(curfile)

	if curfile not in cursors:
		cursors[curfile] = parse_source_file(curfile, builddefs)
		map_source_file(curfile)

	symbol = _get_symbol(curfile, line, col)

	if symbol is not None:
		outputstr = "Symbol(%d, %d): %s - %s (%s)" % (symbol.location.line, symbol.location.column, symbol.spelling, symbol.kind, symbol.spelling in declarations)

		if symbol.kind not in decltypes and symbol.spelling in declarations:
			#decl = declarations[symbol.spelling]
			decl = symbol.referenced

			if decl.kind == CursorKind.FUNCTION_DECL and not decl.is_definition():
				pynano.output_text("Decl not found. Press ENTER to search in folder...")

				filename = _find_real_funcdef(pynano.get_current_file(), decl)

				# pynano.output_text("Found real decl in " + str(filename))

				# Look again
				decl = declarations[symbol.spelling]

			outputstr += " Declared in (%s, %d, %d)" % (decl.location.file.name, decl.location.line, decl.location.column)

			# Check if file is open in a buffer
			if decl.location.file.name is not pynano.get_current_file():
				if decl.location.file.name not in pynano.get_open_buffers():
					# Open the buffer
					pynano.file_open(decl.location.file.name)
				else:
					# Swap to the file
					pynano.file_navigate(decl.location.file.name)

			# Go to the line and column
			pynano.goto_position(decl.location.line, decl.location.column)
		#pynano.output_text(outputstr)

	return

	# Load up the current file
	with open(curfile, "r") as f:
		data = f.read()

	pynano.output_text("%d,%d" % (line, col))

	lines = data.split('\n')

	symbols = lines[line - 1].split(' ')
	#symbol = symbols[col]

	if curfile not in mappedTags:
		return

	#pynano.output_text(str(mappedTags[curfile].keys()))

	if line in mappedTags[curfile]:
		pynano.output_text(str(mappedTags[curfile][line]))

def plugin_setup():
	pynano.kb_subscribe("F12", goto_definition)

def _find_sources(root = ".", files = []):
	for f in os.listdir(root):
		fullpath = os.path.sep.join((root, f))

		if os.path.isdir(f):
			_find_sources(fullpath, files)
		elif f.endswith(".c") or f.endswith(".cpp"):
			files.append(fullpath)

	return files

def _find_makefile(root = ".", files = []):
	for f in os.listdir(root):
		fullpath = os.path.sep.join((root, f))

		if os.path.isdir(f):
			res = _find_makefile(fullpath)
		elif f.endswith("Makefile"):
			files.append(fullpath)

	return files

def find_builddefs(makefile):
	defs = []

	with open(makefile, "r") as f:
		data = f.read()

	for line in data.split("\n"):
		matches = defs_pattern.findall(line)

		for m in matches:
			if m not in defs:
				defs.append(m)

	return defs

def list_includes(node, includes = {}):
	for child in node.get_children():
		#print(child.kind)
		#if child.kind == CursorKind.INCLUSION_DIRECTIVE:
		if child.kind == CursorKind.FUNCTION_DECL:
			if child.location.file != None:
				if child.location.file not in includes:
					includes[child.location.file] = {}
				if child.location.line not in includes[child.location.file]:
					includes[child.location.file][child.location.line] = []
				includes[child.location.file][child.location.line].append(child.displayname)
			list_includes(child, includes)
	return includes

def _get_file(filename, lst = sources):
	# Find the right filename
	for f in lst:
		if filename in f:
			return f

	return None

def _get_symbol(filename, line, column):
	global sources

	filename = _get_file(filename)

	if filename is None:
		# pynano.output_text("Source file [%s] not found" % filename)
		return

	if line not in sources[filename]:
		# print("No symbol in line: %d column: %d" % (line, column))
		return

	# pynano.output_text(str(["%s (%d)" % (n.spelling, n.location.column) for n in sources[filename][line]]))

	symbols = sorted(sources[filename][line], key = lambda x: x.location.column, reverse = True)

	for node in symbols:
		if column + 1 >= node.location.column:
			return node

	return None

def parse_source_file(filename, defines):
	# print(filename, defines)
	return TranslationUnit.from_source(filename, args = ["-I%s" % os.path.dirname(filename), "-I."] + ["-D%s" % d for d in defines])
	return Index.create().parse(filename)

def map_source_file(filename, node = None):
	global sources
	global decltypes

	if node is None:
		#pynano.output_text("Mapping source %s" % filename)
		node = cursors[filename].cursor

	for n in node.get_children():
		if n is None or n.location is None or n.location.file is None:
			continue

		if  n.location.file.name not in sources:
			#print("Mapping %s" % n.location.file.name)
			sources[n.location.file.name] = {}

		if n.location.line not in sources[n.location.file.name]:
			sources[n.location.file.name][n.location.line] = []

		# Index node
		if n.spelling != "":
			sources[n.location.file.name][n.location.line].append(n)

		if n.kind in decltypes:
			# Prefer definitions on declarations
			if n.spelling not in declarations or \
				(n.spelling in declarations and not declarations[n.spelling].is_definition()):
				declarations[n.spelling] = n

		# Recursively find more
		map_source_file(filename, n)

def parse_open_files():
	while True:
		for f in pynano.get_open_buffers():
			_reparse(f)
		time.sleep(10)
		print("Thread")

def main():
	global files
	global cursors
	global sources
	global builddefs
	global parse_thread_running

	for filename in _find_sources():
		with open(filename, "rb") as f:
			files[filename] = sha256sum(f.read())

	makefiles = _find_makefile()

	builddefs = []
	for m in makefiles:
		builddefs += find_builddefs(m)

	# A parse thread
	#_thread.start_new_thread(parse_open_files, ())
	return

	curfile = "./src/history.c"
	cursors[curfile] = parse_source_file(curfile, builddefs)
	map_source_file(curfile)
	#symbol = _get_symbol(curfile, 521, 13)

	#print(dir(symbol))
	#print(symbol.spelling)
	#print(symbol.is_definition())
	#print(symbol.kind)
	#print(declarations[symbol.spelling].kind)
	#print(declarations[symbol.spelling].location.file.name)

	symbol = _get_symbol(curfile, 148, 6)
	decl = declarations[symbol.spelling]

	print(symbol.spelling)
	print(symbol.kind)
	print(symbol.referenced.location.line)

	if decl.kind == CursorKind.FUNCTION_DECL and not decl.is_definition():
		_find_real_funcdef(curfile, decl)
		decl = declarations[symbol.spelling]

	print(decl.kind)
	print(decl.location.file.name)
	print(decl.is_definition())

try:
	plugin_setup()
except:
	print("Error setting up plugins")

main()
