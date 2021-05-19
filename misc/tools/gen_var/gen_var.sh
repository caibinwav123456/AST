for file in `tree -nif --noreport IPCIF`;
 do
  if [ -f "$file" ]; then
   d=`cat "$file" | grep DEFINE_`;
   if [ -n "$d" ]; then
    if [ "$1" = "-v" ]; then
     echo "$file":
	fi;
    echo "$d";
   fi;
  fi;
 done
