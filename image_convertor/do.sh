#!/bin/bash
for img in *.png; do
    
    ffmpeg -i "$img" -vf scale=828:177 "${img%.*}_medium.jpg"
    ffmpeg -i "$img" -vf scale=800:800 "${img%.*}_large.jpg"
  
done