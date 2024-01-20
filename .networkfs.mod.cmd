savedcmd_/home/vboxuser/lab4/networkfs.mod := printf '%s\n'   main.o http.o | awk '!x[$$0]++ { print("/home/vboxuser/lab4/"$$0) }' > /home/vboxuser/lab4/networkfs.mod
