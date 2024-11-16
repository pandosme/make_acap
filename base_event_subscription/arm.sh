rm -rf build
docker build --progress=plain --tag acap .
docker cp $(docker create acap):/opt/app ./build
mv build/*.eap .
rm -rf build
