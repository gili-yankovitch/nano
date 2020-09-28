_punano = None

def __init__(cmodule):
	global _pynano
	_pynano = cmodule

def is_modified(filename):
	if filename not in get_open_buffers():
		return False

	return _pynano.is_modified(filename)

def get_buffer_content(filename):
	return "\n".join(_pynano.get_buffer_content(filename))

def output_text(text):
	_pynano.output_text(text)

def file_open(filename):
	_pynano.file_open(filename)

def file_navigate(filename):
	_pynano.file_navigate(filename)

def get_open_buffers():
	return _pynano.get_open_buffers()

def kb_subscribe(shortcut, callable):
	_pynano.kb_subscribe(shortcut, callable)

def get_current_file():
	return _pynano.current_file()

def get_current_position():
	return _pynano.current_position()

def goto_position(x, y):
	_pynano.goto_position(x, y)

