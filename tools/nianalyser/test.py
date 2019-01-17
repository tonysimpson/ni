import pointbreak
import nianalyser
import threading
import time


class MemoryProfiler:
    def __init__(self, db):
        db.add_breakpoint('.*', self.enter)
        self.file = open('mem.log', 'w')
        self.depth = 0

    def enter(self, db):
        return_address = db.frame.parent.registers.rip
        self.depth += 1
        depth = self.depth
        backtrace = '.'.join(i.function_name for i in db.backtrace())
        start_resident = db.statm.resident
        def exit(db):
            if self.depth == depth:
                self.depth -= 1
                amount = db.statm.resident - start_resident
                if amount != 0:
                    self.file.write('%s %s\n' % (backtrace, amount))
                return False
            else:
                return True
        db.add_breakpoint(return_address, exit, immediately=True)
        return True


class Test:
    def __init__(self):
        self.db = None
        self.nia = None

    def setup_and_run(self):
        #self.db = pointbreak.create_debugger('flask', 'run')
        self.db = pointbreak.create_debugger('python', 'app.py')
        #self.nia = nianalyser.NiAnalyser(self.db)
        stop_count = 0
        for event in self.db:
            if event.name == 'STOP':
                stop_count += 1
                if stop_count == 2:
                    MemoryProfiler(self.db)

                




test = Test()
thread = threading.Thread(target=test.setup_and_run)
thread.daemon = True
thread.start()

last_pct_used = 0
while True:
    if test.db:
        time.sleep(0.1)
        pct_used = (test.db.statm.resident / 2089545728.0) * 100
        if pct_used != last_pct_used:
            print pct_used
            last_pct_used = pct_used
        if pct_used > 15.0:
            test.db.sigstop()
            test.db.sigcont()


