import moviepy.editor as mp

audioClip = mp.AudioFileClip("/home/bq.mp3")
videoClip = mp.VideoFileClip("/home/55.mp4", audio=False)
videoClip2 = videoClip.set_audio(audioClip)
video = mp.CompositeVideoClip([videoClip2])

video.write_videofile("/home/output.mp4", fps=25, codec="mpeg4", bitrate="2000k")
