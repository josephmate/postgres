# create the directory structure
cat files.txt  | xargs dirname | sort | uniq | xargs mkdir -p

# copy the files
cat files.txt  | xargs -i{} cp ../{} {}
