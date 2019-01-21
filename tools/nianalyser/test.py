import pointbreak
import nianalyser

db = pointbreak.create_debugger('python', 'app.py')
nai = nianalyser.NiAnalyser(db)
db.continue_to_last_event()

