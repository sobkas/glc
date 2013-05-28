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
	i386-linux-gnu-glc-play $2 -o - -a $i|avconv -i - -y $tmp/audio_$i.wav
	#audios='-i '$tmp'/audio'$i'.mkv '$audios
	au=$tmp'/audio_'$i'.wav '$au
done
#echo $au
#echo $audios
ls -lh $tmp
i386-linux-gnu-glc-play $2 -o - -y 1|avconv -i - -y $tmp/video.mkv

if [ x$3 != 'xkeep' ];then
	if [ $1 == '1' ];then
		mv $tmp/audio_1.wav $tmp/audio-out.wav
	else
		sox -m $au $tmp/audio-out.wav
		rm $tmp/audio_*.wav
	fi
avconv -y -i $tmp/video.mkv -i $tmp/audio-out.wav -vcodec copy $2.mkv
rm -r $tmp
rm $2
fi
