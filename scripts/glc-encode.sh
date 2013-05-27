#! /usr/bin/env bash
if [ x$1 == 'x' ]
  then
      exit
fi

if [ x$2 == 'x' ]
  then
      exit
fi



tmp=`mktemp -d`
echo $tmp
#mkfifo $tmp/audio.fifo 
#mkfifo $tmp/video.fifo
audios=""
au=""
for i in `seq 1 $1`;
do
	i386-linux-gnu-glc-play $2 -o - -a $i|avconv -i - -y $tmp/audio$i.wav
	#audios='-i '$tmp'/audio'$i'.mkv '$audios
	au=$tmp'/audio'$i'.wav '$au
done
#echo $au
#echo $audios
i386-linux-gnu-glc-play $2 -o - -y 1|avconv -i - -y $tmp/video.mkv

if [ x$3 != 'xkeep' ];then
sox -m $au $tmp/audio-out.wav
avconv -y -i $tmp/video.mkv -i $tmp/audio-out.wav -vcodec copy $2.mkv
rm -r $tmp
rm $2
fi
