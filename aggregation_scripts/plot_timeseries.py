import sys

from matplotlib import pyplot
import numpy as np
import pandas as pd

dt = np.dtype([("Invalid Entries", "<u1")])
data = np.fromfile(sys.argv[0], dtype=dt)

df = pd.DataFrame(data, columns=data.dtype.names)

max_entries = 18 * 64

fix, ax = pyplot.subplots()
ax.axhline(y=max_entries)
ax.axhline(y=df.loc[:, "Invalid Entries"].mean())
ax.plot(df)
pyplot.show()
