g++ $@ -DLINUX fs_shell.cpp get_char_linux.cpp sh_cmd_linux.cpp file.cpp parse_cmd.cpp path.cpp complete.cpp fsenv.cpp utility.cpp sh_main.cpp -Wno-write-strings -o fs_sh;
chmod 777 fs_sh;
