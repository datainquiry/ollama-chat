project('ollama-chat', 'c',
  version : '0.1.1',
  default_options : ['c_std=c11', 'warning_level=2'])

dependencies = [
  dependency('gtk4'),
  dependency('libcurl'),
  dependency('json-c'),
  dependency('gtksourceview-5'),
  dependency('glib-2.0'),
  dependency('threads'),
  dependency('uuid'),
]

sources = files(
  'src/ollama_chat.c',
  'src/ollama_api.c',
  'src/ui.c',
  'src/ui_callbacks.c',
  'src/ui_chat_view.c',
  'src/ui_input.c',
  'src/ui_history.c',
  'src/ui_dialogs.c',
  'src/ui_header.c',
  'src/web_search.c',
  'src/history.c',
  'src/config.c',
  'src/markdown.c',
)

executable('ollama-chat', sources,
  dependencies: dependencies,
  install : true)

install_data('data/ollama-chat.desktop',
  install_dir: get_option('datadir') / 'applications')

install_data('data/ollama-chat.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
  rename: 'dev.datainquiry.ollama-chat.svg')
