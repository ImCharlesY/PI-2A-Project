import numpy as np
import matplotlib.pylab as plt

# square
a=np.hstack([np.ones(50)*50,np.zeros(50)]).astype('int32')
print(a.tolist())
plt.subplot(2,2,1)
plt.stem(a)

# triangle
a=np.arange(50).astype('int32')
b=np.hstack([a,np.fliplr([a])[0]])
print(b.tolist())
plt.subplot(2,2,2)
plt.stem(b)

# sawtooth
b=np.linspace(0,63,num=100).astype('int32')
print(b.tolist())
plt.subplot(2,2,3)
plt.stem(b)

# sin
a=np.around(np.sin(np.arange(50)/100*np.pi)*63,decimals=0).astype('int32')
b=np.hstack([a,np.fliplr([a])[0]])
print(b.tolist())
plt.subplot(2,2,4)
plt.stem(b)

plt.show()