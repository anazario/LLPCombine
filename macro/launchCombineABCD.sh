
#dcdir=datacards
#dcdir=datacards_22j
#dcdir=datacards_11j
#dcdir=datacards_2GLLL
dcdir=datacards
pushd ../
#combineTool.py -M AsymptoticLimits  -d ${dcdir}/*/*.txt --there -n .limit --parallel 4
#combineTool.py -M Significance -t -1 --expectSignal=1 -d ${dcdir}/*/*.txt --there --parallel 4

combineTool.py -M T2W -i ${dcdir}/*/*.txt -o ws.root
combineTool.py -M FitDiagnostics --saveShapes --saveWithUncertainties -d ${dcdir}/*/ws.root --there

popd
