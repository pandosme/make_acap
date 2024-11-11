rm -rf ./build
docker build --build-arg CHIP=artpec7 . -t acap
docker cp $(docker create acap):/opt/app ./build
cp ./build/*.eap .
