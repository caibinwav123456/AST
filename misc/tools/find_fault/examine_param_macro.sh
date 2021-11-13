for file in `tree -nif --noreport IPCIF`;
 do
  if [ -f "$file" ]; then
   d=`cat "$file" | egrep -n '\#define\s+[A-Za-z0-9_]+\('`;
   if [ -n "$d" ]; then
    if [ "$1" == "-v" ]; then
     echo "$file":
    fi;
    echo "$d";
   fi;
  fi;
 done
