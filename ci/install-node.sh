hash=$( brew versions nodejs 2>/dev/null | grep ^0.8.22 | awk -F" " '{print $4}' )
cd $( brew --prefix )
git checkout $hash Library/Formula/node.rb
brew install nodejs
