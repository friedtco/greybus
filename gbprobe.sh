#!/bin/bash

#
# Link this script to gbunprobe.sh and run that to make it remove modules
#

#set -x

declare -A MAP

KOS="$(find * -name '*.ko')"

if [ "" = "${KOS}" ]; then
	exit 0;
fi

for i in ${KOS}; do
	j="$(echo "${i}" | sed -e 's/\.ko//g' -e 's/-/_/g')"
	MAP[$j]="${i}"
done

echo "basename is $(basename "${0}")"
if [ "$(basename "${0}")" = "gbprobe.sh" ]; then
	PROBE=1
else
	PROBE=0
fi

while [ ${#MAP[@]} -gt 0 ]; do

	echo "modules left: ${#MAP[@]}: $(echo "${!MAP[@]}" | sort -u)"

	for i in "${!MAP[@]}"; do
		KEY="${i}"
		VAL="${MAP[${i}]}"

		if [ 1 -eq ${PROBE} ]; then
			#echo "looking for module ${KEY}"

			if [ "$(lsmod | awk '{ print $1 }' | grep "^${KEY}\.ko$")" != "" ]; then
				unset MAP[${KEY}]
				break
			fi

			sudo insmod ${VAL} &> /dev/null
			if [ $? -eq 0 ]; then
				echo "inserted module ${VAL} (${KEY})"
				unset MAP[${KEY}]
			#else
			#	echo "failed to insert module ${VAL}"
			fi
		else
			#echo "looking for module ${KEY}"

			if [ "$(lsmod | awk '{ print $1 }' | grep "${KEY}")" = "" ]; then
				unset MAP[${KEY}]
				break
			fi

			sudo rmmod ${VAL} &> /dev/null
			if [ $? -eq 0 ]; then
				echo "removed module ${VAL} (${KEY})"
				unset MAP[${KEY}]
			#else
			#	echo "failed to remove module ${VAL}"
			fi
		fi
	done
done

exit 0
