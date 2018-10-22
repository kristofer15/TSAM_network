user=$(cat .skeluser)
skel=$(cat .skeladdress)
ssh -t $user@$skel rm -rf TSAM_botnet
scp -r $(pwd) $user@$skel:TSAM_botnet
