ln -s ../fs_shell.cpp fs_shell.cpp;
ln -s ../fs_shell.h fs_shell.h;
ln -s ../parse_cmd.cpp parse_cmd.cpp;
ln -s ../parse_cmd.h parse_cmd.h;
ln -s ../complete.cpp complete.cpp;
ln -s ../complete.h complete.h;
ln -s ../../arch/linux/get_char.cpp get_char.cpp;
ln -s ../../common/path.cpp path.cpp;
ln -s ../../../include/path.h path.h;
ln -s ../../../include/dir_symbol.h dir_symbol.h;
ln -s ../../../include/sys.h sys.h
g++ $@ -DLINUX -I../../../include/arch/linux fs_shell.cpp get_char.cpp sh_cmd_linux.cpp file.cpp parse_cmd.cpp path.cpp complete.cpp utility.cpp sh_main.cpp -Wno-write-strings -o fs_sh;
chmod 777 fs_sh;
rm -f fs_shell.cpp fs_shell.h parse_cmd.cpp parse_cmd.h complete.cpp complete.h get_char.cpp path.cpp path.h dir_symbol.h sys.h;