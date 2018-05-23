def read_blocks():
    f = open('codegen.log')
    while True:
        a_tag, a_ploc, a_cpos = next(f).split()
        b_tag, b_ploc, b_cpos = next(f).split()
        assert a_tag == 'BEGIN_CODE'
        assert b_tag == 'END_CODE'
        yield (a_ploc, a_cpos, b_cpos, int(a_cpos, 16), int(b_cpos, 16))

blocks = []
for block in read_blocks():
    for past_block in blocks:
        if not (block[3] >= past_block[4] or block[4] <= past_block[3]):
            print('%s %s %s overwrites %s %s %s' % (block[0], block[1], block[2], past_block[0], past_block[1], past_block[2]))
    blocks.append(block)

