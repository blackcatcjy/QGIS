/***************************************************************************
  qgspointlocator.h
  --------------------------------------
  Date                 : November 2014
  Copyright            : (C) 2014 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSPOINTLOCATOR_H
#define QGSPOINTLOCATOR_H

class QgsPointXY;
class QgsVectorLayer;

#include "qgis_core.h"
#include "qgsfeature.h"
#include "qgspointxy.h"
#include "qgscoordinatereferencesystem.h"
#include "qgscoordinatetransform.h"

class QgsPointLocator_VisitorNearestVertex;
class QgsPointLocator_VisitorNearestEdge;
class QgsPointLocator_VisitorArea;
class QgsPointLocator_VisitorEdgesInRect;

namespace SpatialIndex SIP_SKIP
{
  class IStorageManager;
  class ISpatialIndex;
}

/**
 * \ingroup core
 * \brief The class defines interface for querying point location:
 *  - query nearest vertices / edges to a point
 *  - query vertices / edges in rectangle
 *  - query areas covering a point
 *
 * Works with one layer.
 *
 * \since QGIS 2.8
 */
class CORE_EXPORT QgsPointLocator : public QObject
{
    Q_OBJECT
  public:

    /**
     * Construct point locator for a \a layer.
     *
     * If a valid QgsCoordinateReferenceSystem is passed for \a destinationCrs then the locator will
     * do the searches on data reprojected to the given CRS. For accurate reprojection it is important
     * to set the correct \a transformContext if a \a destinationCrs is specified. This is usually taken
     * from the current QgsProject::transformContext().
     *
     * If \a extent is not null, the locator will index only a subset of the layer which falls within that extent.
     */
    explicit QgsPointLocator( QgsVectorLayer *layer, const QgsCoordinateReferenceSystem &destinationCrs = QgsCoordinateReferenceSystem(),
                              const QgsCoordinateTransformContext &transformContext = QgsCoordinateTransformContext(),
                              const QgsRectangle *extent = nullptr );

    ~QgsPointLocator() override;

    /**
     * Get associated layer
     * \since QGIS 2.14
     */
    QgsVectorLayer *layer() const { return mLayer; }

    /**
     * Get destination CRS - may be an invalid QgsCoordinateReferenceSystem if not doing OTF reprojection
     * \since QGIS 2.14
     */
    QgsCoordinateReferenceSystem destinationCrs() const;

    /**
     * Get extent of the area point locator covers - if null then it caches the whole layer
     * \since QGIS 2.14
     */
    const QgsRectangle *extent() const { return mExtent; }

    /**
     * Configure extent - if not null, it will index only that area
     * \since QGIS 2.14
     */
    void setExtent( const QgsRectangle *extent );

    /**
     * The type of a snap result or the filter type for a snap request.
     */
    enum Type
    {
      Invalid = 0, //!< Invalid
      Vertex  = 1, //!< Snapped to a vertex. Can be a vertex of the geometry or an intersection.
      Edge    = 2, //!< Snapped to an edge
      Area    = 4, //!< Snapped to an area
      All = Vertex | Edge | Area //!< Combination of vertex, edge and area
    };

    Q_DECLARE_FLAGS( Types, Type )

    /**
     * Prepare the index for queries. Does nothing if the index already exists.
     * If the number of features is greater than the value of maxFeaturesToIndex, creation of index is stopped
     * to make sure we do not run out of memory. If maxFeaturesToIndex is -1, no limits are used. Returns
     * false if the creation of index has been prematurely stopped due to the limit of features, otherwise true */
    bool init( int maxFeaturesToIndex = -1 );

    //! Indicate whether the data have been already indexed
    bool hasIndex() const;

    struct Match
    {
        //! construct invalid match
        Match() = default;

        Match( QgsPointLocator::Type t, QgsVectorLayer *vl, QgsFeatureId fid, double dist, const QgsPointXY &pt, int vertexIndex = 0, QgsPointXY *edgePoints = nullptr )
          : mType( t )
          , mDist( dist )
          , mPoint( pt )
          , mLayer( vl )
          , mFid( fid )
          , mVertexIndex( vertexIndex )
        {
          if ( edgePoints )
          {
            mEdgePoints[0] = edgePoints[0];
            mEdgePoints[1] = edgePoints[1];
          }
        }

        QgsPointLocator::Type type() const { return mType; }

        bool isValid() const { return mType != Invalid; }
        bool hasVertex() const { return mType == Vertex; }
        bool hasEdge() const { return mType == Edge; }
        bool hasArea() const { return mType == Area; }

        /**
         * for vertex / edge match
         * units depending on what class returns it (geom.cache: layer units, map canvas snapper: dest crs units)
         */
        double distance() const { return mDist; }

        /**
         * for vertex / edge match
         * coords depending on what class returns it (geom.cache: layer coords, map canvas snapper: dest coords)
         */
        QgsPointXY point() const { return mPoint; }

        //! for vertex / edge match (first vertex of the edge)
        int vertexIndex() const { return mVertexIndex; }

        /**
         * The vector layer where the snap occurred.
         * Will be null if the snap happened on an intersection.
         */
        QgsVectorLayer *layer() const { return mLayer; }

        /**
         * The id of the feature to which the snapped geometry belongs.
         */
        QgsFeatureId featureId() const { return mFid; }

        //! Only for a valid edge match - obtain endpoints of the edge
        void edgePoints( QgsPointXY &pt1 SIP_OUT, QgsPointXY &pt2 SIP_OUT ) const
        {
          pt1 = mEdgePoints[0];
          pt2 = mEdgePoints[1];
        }

        bool operator==( const QgsPointLocator::Match &other ) const
        {
          return mType == other.mType &&
                 mDist == other.mDist &&
                 mPoint == other.mPoint &&
                 mLayer == other.mLayer &&
                 mFid == other.mFid &&
                 mVertexIndex == other.mVertexIndex &&
                 mEdgePoints == other.mEdgePoints;
        }

      protected:
        Type mType = Invalid;
        double mDist = 0;
        QgsPointXY mPoint;
        QgsVectorLayer *mLayer = nullptr;
        QgsFeatureId mFid = 0;
        int mVertexIndex = 0; // e.g. vertex index
        QgsPointXY mEdgePoints[2];
    };

#ifndef SIP_RUN
    typedef class QList<QgsPointLocator::Match> MatchList;
#else
    typedef QList<QgsPointLocator::Match> MatchList;
#endif

    /**
     * Interface that allows rejection of some matches in intersection queries
     * (e.g. a match can only belong to a particular feature / match must not be a particular point).
     * Implement the interface and pass its instance to QgsPointLocator or QgsSnappingUtils methods.
     */
    struct MatchFilter
    {
      virtual ~MatchFilter() = default;
      virtual bool acceptMatch( const QgsPointLocator::Match &match ) = 0;
    };

    // intersection queries

    /**
     * Find nearest vertex to the specified point - up to distance specified by tolerance
     * Optional filter may discard unwanted matches.
     */
    Match nearestVertex( const QgsPointXY &point, double tolerance, QgsPointLocator::MatchFilter *filter = nullptr );

    /**
     * Find nearest edge to the specified point - up to distance specified by tolerance
     * Optional filter may discard unwanted matches.
     */
    Match nearestEdge( const QgsPointXY &point, double tolerance, QgsPointLocator::MatchFilter *filter = nullptr );

    /**
     * Find nearest area to the specified point - up to distance specified by tolerance
     * Optional filter may discard unwanted matches.
     * This will first perform a pointInPolygon and return first result.
     * If no match is found and tolerance is not 0, it will return nearestEdge.
     * \since QGIS 3.0
     */
    Match nearestArea( const QgsPointXY &point, double tolerance, QgsPointLocator::MatchFilter *filter = nullptr );

    /**
     * Find edges within a specified recangle
     * Optional filter may discard unwanted matches.
     */
    MatchList edgesInRect( const QgsRectangle &rect, QgsPointLocator::MatchFilter *filter = nullptr );
    //! Override of edgesInRect that construct rectangle from a center point and tolerance
    MatchList edgesInRect( const QgsPointXY &point, double tolerance, QgsPointLocator::MatchFilter *filter = nullptr );

    // point-in-polygon query

    // TODO: function to return just the first match?
    //! find out if the point is in any polygons
    MatchList pointInPolygon( const QgsPointXY &point );

    //

    /**
     * Return how many geometries are cached in the index
     * \since QGIS 2.14
     */
    int cachedGeometryCount() const { return mGeoms.count(); }

  protected:
    bool rebuildIndex( int maxFeaturesToIndex = -1 );
  protected slots:
    void destroyIndex();
  private slots:
    void onFeatureAdded( QgsFeatureId fid );
    void onFeatureDeleted( QgsFeatureId fid );
    void onGeometryChanged( QgsFeatureId fid, const QgsGeometry &geom );

  private:
    //! Storage manager
    SpatialIndex::IStorageManager *mStorage = nullptr;

    QHash<QgsFeatureId, QgsGeometry *> mGeoms;
    SpatialIndex::ISpatialIndex *mRTree = nullptr;

    //! flag whether the layer is currently empty (i.e. mRTree is null but it is not necessary to rebuild it)
    bool mIsEmptyLayer;

    //! R-tree containing spatial index
    QgsCoordinateTransform mTransform;
    QgsVectorLayer *mLayer = nullptr;
    QgsRectangle *mExtent = nullptr;

    friend class QgsPointLocator_VisitorNearestVertex;
    friend class QgsPointLocator_VisitorNearestEdge;
    friend class QgsPointLocator_VisitorArea;
    friend class QgsPointLocator_VisitorEdgesInRect;
};


#endif // QGSPOINTLOCATOR_H
