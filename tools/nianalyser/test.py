import pointbreak
import nianalyser

db = pointbreak.create_debugger('python', '-c', 'import ni; ni.engage(); import re')
nia = nianalyser.NiAnalyser(db)
db.continue_to_last_event()
