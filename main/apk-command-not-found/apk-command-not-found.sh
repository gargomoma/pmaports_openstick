#!/usr/bin/sh

printf "%s: command not found\n" "$1"

# find the command if it exists
if apk info "$1" >/dev/null 2>&1;  then
	printf "Would you like to attempt to install it? [y/N]\n"
	read -r result
	if [ "$result" = "y" ] || [ "$result" = "yes" ]; then
		printf "Installing %s..\n" "$1"
		exec sudo apk add cmd:"$1"
	else
		printf "Not installing %s.\n" "$1"
	fi
else
		printf "Command could not be found in repositories.\n"
fi
