user=$(cat .skeluser)
skel=$(cat .skeladdress)

# Remove existing deployments an ignore errors
ssh -t $user@$skel rm -rf TSAM_botnet_G51 2> /dev/null

# Copy current directory to server
scp -r $(pwd) $user@$skel:TSAM_botnet_G51
