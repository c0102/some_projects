#!/bin/sh
####################################
#
# Create apps Backup script.
#
####################################

# What to backup. 
backup_files="main"

# Where to backup to.
#dest="/mnt/backup"

# Create archive filename.
day=$(date +%d-%m-%Y)
#hostname=$(hostname -s)
archive_file="SG-$day"

# Print start status message.
echo "Backing up $backup_files to $dest/$archive_file"
date
echo

# Backup the files using tar.
tar -zcf $archive_file.tar.gz $backup_files
#tar czf $dest/$archive_file $backup_files

cp $archive_file.tar.gz /media/sf_shared/

# Print end status message.
echo
echo "Backup finished"
date

# Long listing of files in $dest to check file sizes.
#ls -lh $dest

#tar -zcvf $filename.tar.gz main/
#cp esp32_ethernet.tar.gz /media/sf_shared/
