for i in sorted((line.split() for line in open('codegen.log')), key=lambda x: x[2]):
    print('\t'.join(reversed(i)))

