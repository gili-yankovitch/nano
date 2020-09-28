#include "prototypes.h"
#include <stdbool.h>
#include <malloc.h>
#include <ncurses.h>

#ifdef ENABLE_PLUGINS

#include <Python.h>

static char stdout_redirect[] =
						"import sys								\n"
						"class CatchOutErr:						\n"
						"	def __init__(self):					\n"
						"		self.outputs = []				\n"
						"	def write(self, txt, **kwargs):		\n"
						"		self.outputs.append(txt)		\n"
						"	def read_out(self):					\n"
						"		s = \"\".join(self.outputs)		\n"
						"		self.outputs = []				\n"
						"		return s						\n"
						"catchOutErr = CatchOutErr() 			\n"
						"sys.stdout = catchOutErr				\n"
						"sys.stderr = catchOutErr				\n";


static unsigned int count_char(char * str, char c)
{
	unsigned int count = 0;

	do
	{
		if (*str == c)
			count++;
	}
	while (*++str);

	return count;
}

#ifdef ENABLE_MULTIBUFFER

static PyObject* py_open_buffer(PyObject *self, PyObject *args)
{
	char * filename;

	if (!PyArg_ParseTuple(args, "s", &filename))
	{
		Py_RETURN_NONE;
	}

	open_buffer(filename, TRUE);

	Py_RETURN_NONE;
}

static PyObject* py_is_buffer_modified(PyObject *self, PyObject *args)
{
	char * filename;
	openfilestruct * head = openfile;
	openfilestruct * searcher = head;

	if (!PyArg_ParseTuple(args, "s", &filename))
	{
		Py_RETURN_NONE;
	}

	// Look for the buffer
	do
	{
		if (!strcmp(searcher->filename, filename))
		{
			if (searcher->modified)
			{
				Py_RETURN_TRUE;
			}
			else
			{
				Py_RETURN_FALSE;
			}
		}

		searcher = searcher->next;
	}
	while (searcher != head);

	Py_RETURN_FALSE;
}

static PyObject* py_switch_buffer(PyObject *self, PyObject *args)
{
	char * filename;
	char message[1024];

	if (!PyArg_ParseTuple(args, "s", &filename))
	{
		Py_RETURN_NONE;
	}

	if (switch_to_open_buffer(filename) < 0)
	{
		sprintf(message, "Invalid buffer name %s", filename);

		print_output(message);
	}

	Py_RETURN_NONE;
}

static PyObject * py_get_open_buffers(PyObject *self, PyObject * args)
{
	int i;
	openfilestruct * head = openfile;
	openfilestruct * searcher = head;
	PyObject * buffers = Py_None;

	if (!(buffers  = PyList_New(0)))
	{
		buffers = Py_None;

		goto error;
	}

	do
	{
		PyObject * filename = PyUnicode_FromString(searcher->filename);

		if (PyList_Append(buffers, filename) < 0)
		{
			size_t list_size = PyList_Size(buffers);

			/* Free all objects */
			for (i = 0; i < list_size; ++i)
			{
				Py_DECREF(PyList_GetItem(buffers, i));
			}

			/* Free current item and list */
			Py_DECREF(filename);
			Py_DECREF(buffers);
			buffers = Py_None;

			goto error;
		}

		searcher = searcher->next;
	}
	while (searcher != head);

error:
	return buffers;
}

static PyObject * py_get_buffer_content(PyObject *self, PyObject * args)
{
	bool found = false;
	char * filename;
	openfilestruct * head = openfile;
	openfilestruct * searcher = head;
	linestruct * current;
	PyObject * lines_list = Py_None;

	if (!PyArg_ParseTuple(args, "s", &filename))
	{
		Py_RETURN_NONE;
	}

	do
	{
		if (!strcmp(searcher->filename, filename))
		{
			found = true;

			break;
		}

		searcher = searcher->next;
	}
	while (searcher != head);

	if (!found)
	{
		Py_RETURN_NONE;
	}

	// Allocate memory for all lines
	current = searcher->filetop;
	lines_list = PyList_New(0);

	while (current)
	{
		PyObject * line = PyUnicode_FromString(current->data);

		if (PyList_Append(lines_list, line) < 0)
		{
			int i;
			size_t list_size = PyList_Size(lines_list);

			for (i = 0; i < list_size; ++i)
			{
				Py_DECREF(PyList_GetItem(lines_list, i));
			}

			Py_DECREF(line);
			Py_DECREF(lines_list);
			lines_list = Py_None;

			goto error;
		}

		current = current->next;
	}

error:
	return lines_list;
}

#endif

struct kb_metadata
{
	int keycode;
	PyObject * function;
};

static PyObject* py_kb_subscribe(PyObject *self, PyObject *args)
{
	char * strkeycode;
	PyObject * func;

	if (!PyArg_ParseTuple(args, "sO", &strkeycode, &func))
	{
		Py_RETURN_NONE;
	}

	if (!PyCallable_Check(func))
	{
		Py_RETURN_NONE;
	}

	add_to_sclist_py(MMAIN, strkeycode, keycode_from_string(strkeycode), func, 0);

	Py_RETURN_NONE;
}

static PyObject* py_current_file(PyObject *self, PyObject *args)
{
	return PyUnicode_FromString(openfile->filename);
}

static PyObject* py_current_position(PyObject *self, PyObject *args)
{
	return PyTuple_Pack(2, PyLong_FromUnsignedLong(openfile->current->lineno), PyLong_FromUnsignedLong(openfile->current_x));
}

static PyObject* py_goto_position(PyObject *self, PyObject *args)
{
	ssize_t line;
	ssize_t col;
	unsigned int tabs_num;

	if (!PyArg_ParseTuple(args, "ll", &line, &col))
	{
		Py_RETURN_NONE;
	}

	do_gotolinecolumn(line, col, FALSE, FALSE);

	/* If there are tabs in the current line, recalculate column and reposition */
	if ((tabs_num = count_char(openfile->current->data, '\t')) > 0)
	{
		col += tabs_num * tabsize;

		do_gotolinecolumn(line, col, FALSE, FALSE);
	}

	edit_refresh();

	Py_RETURN_NONE;
}

static PyObject* py_create_window(PyObject *self, PyObject *args)
{
	char * text;

	if (!PyArg_ParseTuple(args, "s", &text))
	{
		Py_RETURN_NONE;
	}

	print_output(text);

	Py_RETURN_NONE;
}

static PyMethodDef hello_methods[] = {
#ifdef ENABLE_MULTIBUFFER
										{
											"file_open", py_open_buffer, METH_VARARGS,
											"Open a new file."
										},
										{
											"file_navigate", py_switch_buffer, METH_VARARGS,
											"Navigate to an open file."
										},
										{
											"get_open_buffers", py_get_open_buffers, METH_NOARGS,
											"retreive a list of open files."
										},
										{
											"is_modified", py_is_buffer_modified, METH_VARARGS,
											"retreive a modified state of an open buffer."
										},
#endif
										{
											"kb_subscribe", py_kb_subscribe, METH_VARARGS,
											"Bind a keyboard shortcut to a function."
										},
										{
											"current_file", py_current_file, METH_NOARGS,
											"Current open file."
										},
										{
											"current_position", py_current_position, METH_VARARGS,
											"Current cursor position."
										},
										{
											"get_buffer_content", py_get_buffer_content, METH_VARARGS,
											"Return a given buffer's content."
										},
										{
											"output_text", py_create_window, METH_VARARGS,
											"Create a new ncurses window"
										},
										{
											"goto_position", py_goto_position, METH_VARARGS,
											"Navigate cursor to line, column in current buffer"
										},
										{NULL, NULL, 0, NULL}
									};
static struct PyModuleDef pynano_def = {
												PyModuleDef_HEAD_INIT,
												"_pynano",
												"Internal nano module",
												-1,
												hello_methods
											};

static PyObject * m = NULL;
static PyObject * stdout_catcher = NULL;
static PyObject * pynano_str = NULL;
static PyObject * _pynano = NULL;
static PyObject * pynano_module = NULL;

void plugins_init(void)
{
	Py_Initialize();
	PyObject * __init__str = NULL;
	PyObject * pynano__init__ = NULL;

	m = PyImport_AddModule("__main__");
	pynano_str = PyUnicode_FromString("_pynano");
	_pynano = PyModule_Create(&pynano_def);
	PyObject_SetAttr(m, pynano_str, _pynano);

	/* Import the pynano module */
	pynano_module = PyImport_ImportModule("pynano");

	if (!pynano_module)
	{
		jot_error(N_("pynano module not installed."));

		return;
	}

	/* Find the __init__ function */
	__init__str = PyUnicode_FromString("__init__");
	pynano__init__ = PyObject_GetAttr(pynano_module, __init__str);
	PyObject_CallFunction(pynano__init__, "O", _pynano);

	// PyRun_SimpleString("import plugin_example;print(plugin_example.__file__)");

	/* Release ref counts */
	Py_DECREF(pynano__init__);
	Py_DECREF(__init__str);
	// while (1);

	/* Grab stdout */
	PyRun_SimpleString(stdout_redirect);

	stdout_catcher = PyObject_GetAttrString(m, "catchOutErr");
}

void plugins_destruct(void)
{
	Py_DECREF(pynano_module);
	Py_DECREF(stdout_catcher);
	Py_DECREF(_pynano);
	Py_DECREF(pynano_str);
	Py_DECREF(m);
}

void load_one_plugin(char * plugin_name)
{
	PyObject * module;
	PyObject * module_name;

	// fprintf(stderr, "Importing module %s\r\b", plugin_name);

	if (!(module = PyImport_ImportModule(plugin_name)))
	{
		jot_error(N_("Invalid module %s"), plugin_name);
	}

	/* Add it to globals */
	module_name = PyUnicode_FromString(plugin_name);
	PyObject_SetAttr(m, module_name, module);
}

void print_init_strings()
{
	PyObject * pymsg;
	char * str;

	pymsg = PyObject_CallMethod(stdout_catcher, "read_out", NULL);
	str = _PyUnicode_AsString(pymsg);

	print_output(str);

	Py_DECREF(pymsg);
}

void print_output(char * output)
{
	WINDOW * win;
	int kbinput = ERR;
	int i;
	char * lines = mallocstrcpy(NULL, output);
	char * curline = lines;
	char exit_message[] = "Press ENTER to close window";
	int exit_message_size = breadth(exit_message);
	int text_size = MAX(breadth(output), exit_message_size);
	int lines_num = (count_char(output, '\n') ?: 1) + 2;

	win = newwin(lines_num + 2, text_size + 4, editwinrows / 2  - lines_num / 2 - 1, COLS / 2 - text_size / 2 - 1);

	/* Draw line-by-line */
	for (i = 0; i < lines_num; ++i)
	{
		char * end_of_line = strchr(curline, '\n');

		if (end_of_line)
			*end_of_line = '\0';

		mvwaddstr(win, 1 + i, 2, curline);

		if (!end_of_line)
			break;

		curline = end_of_line + 1;
	}

	free(lines);

	mvwaddstr(win, lines_num, text_size / 2 - exit_message_size / 2 + 2, exit_message);
	box(win, 0, 0);
	wrefresh(win);

	do
	{
		if ((kbinput = wgetch(edit)) == '\r')
			break;

		wrefresh(edit);
	}
	while (TRUE);

	delwin(win);
	window_init();

	draw_all_subwindows();
}

char message[10000];
void do_plugins(void)
{
	answer = mallocstrcpy(answer, "");

	do
	{
		int response = do_prompt(MWHEREIS,
							answer, &plugins_history, edit_refresh, ">>");

		switch (response)
		{
			case 0:
			{
				PyObject * pymsg;
				char * str;


				if (*answer != '\0')
				{
					update_history(&plugins_history, answer);
				}

				if (!strcmp(answer, "credits"))
					do_credits();
				else
					PyRun_SimpleString(answer);

				pymsg = PyObject_CallMethod(stdout_catcher, "read_out", NULL);
				str = _PyUnicode_AsString(pymsg);

				Py_DECREF(pymsg);

				if ((str) && (*str != '\0'))
				{
					sprintf(message, "Output: %s", _(str));
					print_output(_(str));
				}

				return;
			}

			case 3:
			{
				statusbar(_("Cancelled"));

				return;
			}

			default:
			{
				char message[1024];

				sprintf(message, "Key code: %d", response);

				statusbar(_(message));

				return;
			}
		}
	} while (true);
}

#endif
