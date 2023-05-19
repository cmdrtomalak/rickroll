cmd_/home/bandit/dev/SimplestLKM/hello.mod := printf '%s\n'   hello.o | awk '!x[$$0]++ { print("/home/bandit/dev/SimplestLKM/"$$0) }' > /home/bandit/dev/SimplestLKM/hello.mod
