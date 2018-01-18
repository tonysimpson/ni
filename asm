echo $@
echo 'asm("'$@'");' | gcc -O3 -c -xc - -o/tmp/adsfdasf; objdump -d /tmp/adsfdasf; rm /tmp/adsfdasf
