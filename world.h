#include <iostream>
#include <algorithm>
#include <array>
#include <sstream>
#include <stack>
#include <set>
#include "HTTPRequest.h"

struct coords
{
  int x;
  int z;
};

bool operator<(const coords& c1, const coords& c2)
{
  return c1.x < c2.x;
}

bool operator==(const coords& c1, const coords& c2)
{
  return c1.x == c2.x;
}

namespace world
{

  long getCoordinateValue(int x, int z) 
  {
    return (x * x * 0x4c1906) + (x * 0x5ac0db) + (z * z) * 0x4307a7L + (z * 0x5f24f);
  }

  class World
  {
    private:
      long* cached_coordinate_values;
      long long int seed;
      bool logging = false;
      bool returnOnlyRectangles = true;
      bool allowOneWides = true;
      int spacing = 1;
      int radius = 0;
      int min_size = 8;
      int cache_size = 0;

      void search();
      coords findLargestRect(std::vector<int>);
      bool isSlimeChunk(long cached_coordinate_value, long long int seed);
      void getCluster(int x, int z, long coordinate_value, int depth = 0);
      std::set<std::vector<coords>> slime_clusters;
      coords createSubMatrixHistogram(std::vector<std::vector<bool>> chunks);
      std::vector<std::vector<bool>> generateClusterRegion(std::vector<coords>);

    public:
      void printMap(int radius);
      void printCluster(std::vector<coords> chunks);

      World(long long int seed, int radius, int min_size, int spacing, bool logging, bool returnOnlyRectangles, long* cached_coordinate_values, const int cache_size, bool allowOneWides) {
        this->min_size = min_size;
        this->cache_size = cache_size;
        this->allowOneWides = allowOneWides;
        this->cached_coordinate_values = cached_coordinate_values;
        this->returnOnlyRectangles = returnOnlyRectangles;
        this->logging = logging;
        this->spacing = spacing;
        this->seed = seed;
        this->radius = radius;
        this->search();
      }
  };

  // Determine if there is a slime chunk at x, z
  bool World::isSlimeChunk(long cached_coordinate_value, long long int seed) 
  {
    return !((((((cached_coordinate_value + seed ^ 0x3ad8025fL ^ 0x5DEECE66DL) & 0xFFFFFFFFFFFF) * 0x5DEECE66DL + 0xBL) & 0xFFFFFFFFFFFF) >> 17) % 10);
  }

  // Search radius around 0, 0 for slime chunks
  void World::search()
  {
    int index = 0;
    int half_radius = this->radius / 2;
    for (int z = -half_radius; z < half_radius; z+=this->spacing)
      for (int x = -half_radius; x < half_radius; x+=this->spacing)
        this->getCluster(x, z, this->cached_coordinate_values[index++], 1);
  }

  // https://www.youtube.com/watch?v=g8bSdXCG-lA
  coords World::createSubMatrixHistogram(std::vector<std::vector<bool>> chunks)
  {
    // Initialize array and set all values to 0
    std::vector<int> histogram(chunks[0].size(), 0);
    coords maxDimensions{0, 0};

    // increment each element if multiple y chunks ( like histogram values )
    for (int x = 0; x < chunks.size(); x++)
    {
      for (int z = 0; z < chunks[0].size(); z++)
      {
        // If slime chunk
        if (chunks[x][z])
          histogram[z]++;
        else
          histogram[z]=0;
      }
      
      // Find largest rectangle in region cluster
      coords temp = this->findLargestRect(histogram);
      if (temp.x * temp.z > maxDimensions.x * maxDimensions.z) maxDimensions = temp;
    }

    return maxDimensions;
  }

  // Find largest rect in submatrix histogram and return dimensions
  coords World::findLargestRect(std::vector<int> histogram)
  {
    int h;
    int i;
    int tempH;
    int tempPos = 0;
    int tempSize;
    coords dimensions;
    double maxSize = -std::numeric_limits<double>::infinity();
    std::stack<int> pStack;
    std::stack<int> hStack;

    for (i = 0; i < histogram.size(); i++)
    {
      h = histogram[i];

      if (!hStack.size() || h > hStack.top()) {
        hStack.push(h);
        pStack.push(i);
      } else if (h < hStack.top()) {
        while (!hStack.empty() && h < hStack.top()) {
          tempH = hStack.top();
          tempPos = pStack.top();
          hStack.pop();
          pStack.pop();
          tempSize = tempH * (i - tempPos);
          maxSize = std::max((double)tempSize, maxSize);
          if (maxSize == tempSize) dimensions = coords{i - tempPos, tempH};
        }
        hStack.emplace(h);
        pStack.emplace(tempPos);
      }
    }

    while (!hStack.empty()) {
      tempH = hStack.top();
      tempPos = pStack.top();
      hStack.pop();
      pStack.pop();
      tempSize = tempH * (i - tempPos);
      maxSize = std::max((double)tempSize, maxSize);
      if (maxSize == tempSize) dimensions = coords{i - tempPos, tempH};
    }
    return dimensions;
  }

  // Recursively search for nearby slime chunks within cluster and return dimensions
  void World::getCluster(int x, int z, long coordinate_value, int depth)
  {
    // Holds coordinates of already checked chunks 
    static std::vector<coords> checked_chunks;
    
    // If not slime chunk and checked_chunks does not include these coordinates ( chunk hasn't been checked ), return false;
    if (this->isSlimeChunk(coordinate_value, this->seed) && !std::any_of(checked_chunks.begin(), checked_chunks.end(), [x, z](coords c1){ return(c1.x == x && c1.z == z);})) {
      coords current_coords = coords{x, z};

      // If initial cluster check, clear checked_chunks
      if (depth)
        checked_chunks.clear();

      // Push self to checked chunks
      checked_chunks.push_back(current_coords);

      // Check sides ( This won't access cached coordinate values ( So they have to be calculated at "runtime" ))
      getCluster(x+1, z, getCoordinateValue(x+1, z));
      getCluster(x-1, z, getCoordinateValue(x-1, z));
      getCluster(x, z+1, getCoordinateValue(x, z+1));
      getCluster(x, z-1, getCoordinateValue(x, z-1));

      // Add cluster to set if size >= min_size
      if (checked_chunks.size() >= this->min_size && depth) {

        // Sort cluster array to prevent duplicates
        std::stable_sort(checked_chunks.begin(), checked_chunks.end());
        auto it_res = this->slime_clusters.insert(checked_chunks);

        // Find largest rect dimensions within cluster region
        std::vector<std::vector<bool>> cluster_region = this->generateClusterRegion(checked_chunks);
        coords largest_rect_dimensions = this->createSubMatrixHistogram(cluster_region);

        // If should return cluster
        bool should_return_cluster = (this->returnOnlyRectangles ? largest_rect_dimensions.x * largest_rect_dimensions.z > this->min_size : checked_chunks.size() > this->min_size);

        // If should return one wide clusters
        bool allowOneWides = this->allowOneWides ? true : (largest_rect_dimensions.x != 1 && largest_rect_dimensions.z != 1);

        // Only call if not duplicate
        if (it_res.second && this->logging && should_return_cluster && allowOneWides) {
          int largest_area = largest_rect_dimensions.x * largest_rect_dimensions.z;
          std::cout << "Seed: " << this->seed << std::endl;
          std::cout << "Chunks: (" << x << ", " << z << ")" << std::endl;
          std::cout << "Coordinates: (" << x*16 << ", " << z*16 << ")" << std::endl;

          if (this->returnOnlyRectangles) {
            std::cout << "Size: " << largest_area << std::endl;
          } else {
            std::cout << "Size: " << checked_chunks.size() << std::endl;
          }

          this->printCluster(checked_chunks);
          std::cout << "-----------------------------------------------" << std::endl;

          // Return cluster if area >= this->min_size
          if (largest_area >= this->min_size) {
            // Post to database
            try
            {
              http::Request request("http://149.28.75.54/api");
              std::string chunks_string;
              std::string params;

              // Convert vector of coords to vector of strings
              for(coords c : checked_chunks)
                chunks_string = chunks_string + "{\"x\": " + std::to_string(c.x) + ", \"z\": " + std::to_string(c.z) + "}, ";

              // Trim end of string
              chunks_string = chunks_string.substr(0, chunks_string.length()-2);
              chunks_string = "[" + chunks_string + "]";

              params = params +
                "{\"seed\": \"" + std::to_string(this->seed) + "\"" +
                " ,\"chunks\": " + chunks_string +
                " ,\"coords\": " + "{\"x\": " + std::to_string(checked_chunks[0].x << 4) + ", \"z\": " + std::to_string(checked_chunks[0].z << 4) + "}" + 
                " ,\"size\": " + std::to_string(largest_area) +
                "}";

              std::cout << params;


              // Send request
              const auto response = request.send("POST", params, {
                  "Content-Type: application/json"
              });
              std::cout << std::string{response.body.begin(), response.body.end()} << '\n'; // print the result

            }
            catch (const std::exception& e)
            {
              std::cerr << "Request failed, error: " << e.what() << '\n';
            }
          }
        }
      }
    }
  }

  // Generate 2D array representation of cluster
  std::vector<std::vector<bool>> World::generateClusterRegion(std::vector<coords> chunks)
  {
    // Initialize bounding box
    int top = chunks.front().x;
    int bottom = top;
    int left = chunks.front().z;
    int right = left;

    // Find bounding box
    for (coords &c : chunks) {
      left = std::min(c.z, left);
      right = std::max(c.z, right);
      top = std::max(c.x, top);
      bottom = std::min(c.x, bottom);
    }

    // Bounding box dimensions
    int width = right + 1 - left;
    int height = top + 1 - bottom;

    // Create 2D array representation of chunk cluster
    std::vector<std::vector<bool>> cluster(width, std::vector<bool>(height, 0));

    // Set cluster 
    for (int z = 0; z < width; z++)
    {
      for (int x = 0; x < height; x++)
      {
        coords c{x+bottom, z+left};
        bool chunks_contains = std::any_of(chunks.begin(), chunks.end(), [c](coords c1){ return(c1.x == c.x && c1.z == c.z);});
        cluster[z][x] = chunks_contains;
      }
    }
    return cluster;
  }

  // Display chunk cluster
  void World::printCluster(std::vector<coords> chunks)
  {
    std::vector<std::vector<bool>> cluster = this->generateClusterRegion(chunks);

    // Display cluster
    for (int i = 0; i < cluster.size(); i++) 
    {
      for (int j = 0; j < cluster[0].size(); j++) 
      {
        std::cout << (cluster[i][j] ? "■ " : "□ ");
      }
      std::cout << std::endl;
    }
  }


  // Print map to console
  void World::printMap(int radius)
  {
    int half_radius = radius / 2;
    for (int z = -half_radius; z < half_radius; z+=this->spacing)
    {
      for (int x = -half_radius; x <= half_radius; x+=this->spacing)
      {
         if (z == -half_radius) {
          // Print x coordinates
          std::cout << (x >= 0 ? " " : "") << x << " ";
        } else {
          // Display chunk type
          std::cout << (isSlimeChunk(x, z) ? "■ " : "□ ");
        }
      }
      std::cout << std::endl;
    }

    for (int z = -half_radius; z < half_radius; z++)
      std::cout << z << " ";
  }
};

