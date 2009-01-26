/*

The MIT License

Copyright (c) 1997-2009 Center for the Simulation of Accidental Fires and 
Explosions (CSAFE), and  Scientific Computing and Imaging Institute (SCI), 
University of Utah.

License for the specific language governing rights and limitations under
Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

*/




#ifndef STEALTH_H
#define STEALTH_H 1

#include <Packages/rtrt/Core/Camera.h>

#include <Core/Geometry/Point.h>
#include <Core/Geometry/Vector.h>

#include <sgi_stl_warnings_off.h>
#include <vector>
#include <string>
#include <sgi_stl_warnings_on.h>

namespace rtrt {

using SCIRun::Vector;
using SCIRun::Point;

using std::vector;
using std::string;

// "Stealth" comes from the DoD term for an invisible watcher on the
// simulated battlefield.  The stealths are used in relation to
// cameras.  They contain movement information, but the camera
// contains direction information.
  
class Stealth {

  ////////////////////////////////////////////////////////
  // Stealth Movement
  //
  //   This camera does NOT move like an airplane.
  // The driver (user) can specify movement in the following
  // manner.
  //
  //     +  Accelerate Forward
  //     -  Accelerate Backward
  //     <- Begin turning left 
  //     -> Begin turning right
  //     ^  Begin pitching forward (look down)
  //     v  Begin pitching backward (look up)
  //     7 (keypad) Accelerate to the left
  //     9 (keypad) Accelerate to the right
  //     0 (keypad) Stop ALL movement
  //     . (keypad) Slow down (in all movements (including turns))
  //
  //   Acceleration is in respect to the direction that the eye is
  // looking.  

public:

  // rotate_scale of '4' is good 
  // translate_scale of '100' is good if the eye is 3 units (or so) from
  // the area of interest and the area of interest is ~6 units across.
  Stealth( double translate_scale, double rotate_scale, double gravity_force );
  ~Stealth();

  // Tells the eye (camera) to update its position based on its current
  // velocity vector.
  void updatePosition();

  double getSpeed( int direction ) const;

  // Slows down in all dimensions (pitch, speed, turn, etc);
  void slowDown();
  void stopAllMovement();
  void stopPitchAndRotate();
  void stopPitch();
  void stopRotate();

  void slideLeft();
  void slideRight();

  void goUp();
  void goDown();

  void accelerate();
  void decelerate();
  void turnRight();
  void turnLeft();
  void pitchUp();
  void pitchDown();

  // If gravity is on, the stealth/camera will "fall" towards the ground.
  void toggleGravity();

  // Stealths and Cameras are highly integrated right now... perhaps
  // stealth should be a sub class in camera?  This needs more thought.

  bool   gravityIsOn() { return gravity_on_; }
  double getGravityForce() { return gravity_force_; }

  bool   moving(); // Returns true if moving in any direction

  void updateRotateSensitivity( double scalar );
  void updateTranslateSensitivity( double scalar );

  // Display the Stealth's speeds, etc.
  void print();

  // Returns next location in the path and the new view vector.
  void getNextLocation( Camera* new_camera );

  // Moves the stealth to the next Marker in the path, point/lookat
  // are set to the correct locations.  Index of that pnt is returned.
  // Returns -1 if no route!

  //void using_catmull_rom(vector<Point> &points, vector<Point> &f_points);
  Camera using_catmull_rom(vector<Camera> &cameras, int i, float t);
  Point using_catmull_rom(vector<Point> &points, int i, float t);

  int  getNextMarker( Camera* new_camera);
  int  getPrevMarker( Camera* new_camera);
  int  goToBeginning( Camera* new_camera);
  void getRouteStatus( int & pos, int & numPts );

  // Clear out path stealth is to follow.
  void clearPath();
  // Adds point to the end of the route
  void addToPath( const Camera* new_camera);
  // Adds point in front of the current marker.
  void addToMiddleOfPath( const Camera* new_camera);
  void deleteCurrentMarker();

  // Returns the name of the route (for GUI to display)
  string loadPath( const string & filename );
  void   newPath(  const string & routeName );
  void   savePath( const string & filename );

  int    loadOldPath( const string & filename );
  void   saveOldPath( const string & filename );

  
  // Choose the current path to follow.
  void   selectPath( int index );

private:

  void increase_a_speed( double & speed, int & accel_cnt, double scale, double base, double max );
  void decrease_a_speed( double & speed, int & accel_cnt, double scale, double base, double min );

  void displayCurrentRoute();

  // Scale is based on the size of the "universe".  It effects how fast
  // the stealth will move.

  double baseTranslateScale_;
  double baseRotateScale_;

  double translate_scale_;
  double rotate_scale_;

  // Speeds (in units per frame)
  double speed_;            // + forward, - backward
  double horizontal_speed_; // + right, - left
  double vertical_speed_;   // + up, - down
  double pitch_speed_;      // + down, - up
  double rotate_speed_;     // + right, - left

  // Acceleration counts represent the number of velocity change 
  // requests that the user has made.  They are used to judge how
  // much velocity to add to the velocity vector.  The more requests,
  // the faster we "accelerate" for the next similar request.
  int accel_cnt_;            // + forward, - backward. 
  int horizontal_accel_cnt_; // + right, - left
  int vertical_accel_cnt_;   // + up, - down
  int pitch_accel_cnt_;      // + up, - down
  int rotate_accel_cnt_;     // + right, - left

  // Path information (second draft that uses actuall Cameras instead
  // of just eye and lookat points)

  vector< vector<Camera> >  paths_;
  vector< string >          routeNames_;

  vector<Camera>    * currentPath_;
  string            * currentRouteName_;

  double        segment_percentage_;

  bool          gravity_on_;
  double        gravity_force_;
};

} // end namespace rtrt

#endif
