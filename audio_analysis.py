from pydub import AudioSegment
import matplotlib.pyplot as plt
import pickle
from os import path

mins10 = 0

loudness = []
filePath = ""

sound = AudioSegment.from_file(filePath) #, start_second=0, duration=60)
loudness += [i.rms for i in sound]

del sound

amor = 100
amortizedLoudness = [sum(loudness[i:i + amor]) / amor for i in
                     range((len(loudness) - amor))]

thresholdsNormal = [700, 1500, 2500]
thresholdsTraining = [700, 1500, 2500]

thresholdsLineIn = [1500, 3500, 5800]

thresholds = thresholdsTraining

firstMini = 1500
firstWindow = 500

plusWindowStart = 1000
plusWindowEnd = 1400

indexes = []
values = []


def seek(l, s, e):
    maxi = l[s]
    index = s
    for i in range(index + 1, e):
        if l[i] > maxi:
            index = i
            maxi = l[i]
    return index, maxi


def prettyPrintThresholds(v):
    print(filePath)
    a = len([i for i in v if i < thresholds[0]])
    print(str(a) + " (" + str(100*a/len(v)) + "%) less than " + str(thresholds[0]))
    for j in range(1, len(thresholds)):
        a = len([i for i in v if thresholds[j-1] <= i < thresholds[j]])
        print(str(a) + " (" + str(100*a/len(v)) + "%) between " + str(thresholds[j - 1]) + " and " + str(thresholds[j]))
    a = len([i for i in v if i > thresholds[-1]])
    print(str(a) + " (" + str(100*a/len(v)) + "%) more than " + str(thresholds[-1]))

def p():
    plt.figure()
    plt.plot(values)

ptr = seek(amortizedLoudness, 0, 5000)
indexes.append(ptr[0])
values.append(ptr[1])
while ptr[0] + plusWindowEnd < len(amortizedLoudness):
    ptr = seek(amortizedLoudness, ptr[0] + plusWindowStart, ptr[0] + plusWindowEnd)
    indexes.append(ptr[0])
    values.append(ptr[1])

over100 = []
def oneifNotWhiff(v):
    return 0 if v >= thresholds[1] and v < thresholds[2] else 1

over100.append(sum([oneifNotWhiff(x) for x in values[:100]]))

for i in range(0, len(values)-100):
    over100.append(over100[-1] + oneifNotWhiff(values[i+100]) - oneifNotWhiff(values[i]))

prettyPrintThresholds(values)
