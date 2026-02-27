savedcmd_Main.mod := printf '%s\n'   Main.o | awk '!x[$$0]++ { print("./"$$0) }' > Main.mod
