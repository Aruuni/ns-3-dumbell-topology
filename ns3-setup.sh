echo "installing dependencies"
sudp apt install cmake gnuplot
echo "cloning ns3"
git clone https://gitlab.com/nsnam/ns-3-dev.git
cp SimulatorScript.cc ns-3-dev/scratch/
cd ns-3-dev
./ns3 configure --build-profile=optimized 
echo "Building ns3"
./ns3
echo "DONE"
