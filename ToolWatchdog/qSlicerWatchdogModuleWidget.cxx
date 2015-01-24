/*==============================================================================

Program: 3D Slicer

Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

==============================================================================*/

// Qt includes
#include <QDebug>
#include <QMenu>
#include <QtGui>
#include <QHash>

#include "qSlicerModuleManager.h"
#include "qSlicerApplication.h"

// SlicerQt includes
#include "qSlicerWatchdogModuleWidget.h"
#include "qMRMLWatchdogToolBar.h"
#include "ui_qSlicerWatchdogModuleWidget.h"

#include "vtkMRMLDisplayableNode.h"
#include "vtkSlicerWatchdogLogic.h"
#include "QVTKSlicerWatchdogLogicInternal.h"
#include "vtkMRMLWatchdogNode.h"

#include <vtkCollection.h>
#include <vtkCollectionIterator.h>

#include <limits>

int TOOL_LABEL_COLUMN = 0;
int TOOL_NAME_COLUMN = 1;
int TOOL_SOUND_COLUMN = 2;
int TOOL_TIMESTAMP_COLUMN = 3;
int TOOL_COLUMNS = 4;

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ExtensionTemplate
class qSlicerWatchdogModuleWidgetPrivate: public Ui_qSlicerWatchdogModuleWidget
{
  Q_DECLARE_PUBLIC( qSlicerWatchdogModuleWidget ); 

protected:
  qSlicerWatchdogModuleWidget* const q_ptr;
public:
  qSlicerWatchdogModuleWidgetPrivate( qSlicerWatchdogModuleWidget& object );
  vtkSlicerWatchdogLogic* logic() const;
};

//-----------------------------------------------------------------------------
// qSlicerWatchdogModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerWatchdogModuleWidgetPrivate::qSlicerWatchdogModuleWidgetPrivate( qSlicerWatchdogModuleWidget& object )
: q_ptr( &object )
{
  //QDebug() << "Initialize watchdog widget private!";
}

vtkSlicerWatchdogLogic* qSlicerWatchdogModuleWidgetPrivate::logic() const
{
  Q_Q( const qSlicerWatchdogModuleWidget );
  return vtkSlicerWatchdogLogic::SafeDownCast( q->logic() );
}

//-----------------------------------------------------------------------------
// qSlicerWatchdogModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerWatchdogModuleWidget::qSlicerWatchdogModuleWidget(QWidget* _parent)
: Superclass( _parent )
, d_ptr( new qSlicerWatchdogModuleWidgetPrivate ( *this ) )
{
  //QDebug() << "Initialize watchdog widget!";
  CurrentCellPosition[0]=0;
  CurrentCellPosition[1]=0;
}

//-----------------------------------------------------------------------------
qSlicerWatchdogModuleWidget::~qSlicerWatchdogModuleWidget()
{
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::setup()
{
  Q_D(qSlicerWatchdogModuleWidget);
  //QDebug() << "Setup watchdog widget!";

  d->setupUi(this);
  this->Superclass::setup();

  this->setMRMLScene( d->logic()->GetMRMLScene() );

  connect( d->ModuleNodeComboBox, SIGNAL( currentNodeChanged( vtkMRMLNode* ) ), this, SLOT( onWatchdogNodeChanged() ) );

  connect( d->ModuleNodeComboBox, SIGNAL(nodeAddedByUser(vtkMRMLNode* )), this, SLOT(onWatchdogNodeAddedByUser(vtkMRMLNode* ) ) );
  connect( d->ModuleNodeComboBox, SIGNAL(nodeAboutToBeRemoved(vtkMRMLNode* )), this, SLOT(onModuleNodeAboutToBeRemoved(vtkMRMLNode* ) ) );

  connect( d->ToolbarVisibilityCheckBox, SIGNAL( clicked() ), this, SLOT( onToolbarVisibilityButtonClicked() ) );
  d->ToolbarVisibilityCheckBox->setChecked(true);

  connect( d->StatusRefreshRateSpinBox, SIGNAL( valueChanged(int) ), this, SLOT( onStatusRefreshRateSpinBoxChanged(int) ) );

  connect( d->ToolComboBox, SIGNAL( currentNodeChanged( vtkMRMLNode* ) ), this, SLOT( updateWidget() ) );

  connect( d->AddToolButton, SIGNAL( clicked() ), this, SLOT( onToolNodeAdded() ) );
  connect( d->DeleteToolButton, SIGNAL( clicked() ), this, SLOT( onDeleteButtonClicked()) );
  d->DeleteToolButton->setIcon( QIcon( ":/Icons/MarkupsDelete.png" ) );
  connect( d->UpToolButton, SIGNAL( clicked() ), this, SLOT( onUpButtonClicked()) );
  d->UpToolButton->setIcon( QIcon( ":/Icons/Up.png" ) );
  connect( d->DownToolButton, SIGNAL( clicked() ), this, SLOT( onDownButtonClicked()) );
  d->DownToolButton->setIcon( QIcon( ":/Icons/Down.png" ) );

  connect(d->ToolsTableWidget, SIGNAL(itemDoubleClicked(QTableWidgetItem *)), this, SLOT( onTableItemDoubleClicked() ));

  d->ToolsTableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
  connect( d->ToolsTableWidget, SIGNAL( customContextMenuRequested(const QPoint&) ), this, SLOT( onToolsTableContextMenu(const QPoint&) ) );

  connect( d->logic()->GetQVTKLogicInternal(), SIGNAL( updateTable() ), this, SLOT( onTimeout() ) );

  this->updateFromMRMLNode();
}

void
qSlicerWatchdogModuleWidget
::enter()
{
  Q_D(qSlicerWatchdogModuleWidget);
  //QDebug() << "Enter watchdog widget!";

  if ( this->mrmlScene() == NULL )
  {
    qCritical() << "Invalid scene!";
    return;
  }

  // Create a module MRML node if there is none in the scene.
  //this->topLevelWidget()
  vtkMRMLNode* node = this->mrmlScene()->GetNthNodeByClass(0, "vtkMRMLWatchdogNode");
  if ( node == NULL )
  {
    vtkSmartPointer< vtkMRMLWatchdogNode > newNode = vtkSmartPointer< vtkMRMLWatchdogNode >::New();
    this->mrmlScene()->AddNode( newNode );
  }

  node = this->mrmlScene()->GetNthNodeByClass( 0, "vtkMRMLWatchdogNode" );
  if ( node == NULL )
  {
    qCritical( "Failed to create module node" );
    return;
  }

  // For convenience, select a default module.
  if ( d->ModuleNodeComboBox->currentNode() == NULL )
  {
    d->ModuleNodeComboBox->setCurrentNodeID( node->GetID() );
  }

  vtkCollection* watchdogNodes = this->mrmlScene()->GetNodesByClass( "vtkMRMLWatchdogNode" );
  vtkCollectionIterator* watchdogNodeIt = vtkCollectionIterator::New();
  watchdogNodeIt->SetCollection( watchdogNodes );
  for ( watchdogNodeIt->InitTraversal(); ! watchdogNodeIt->IsDoneWithTraversal(); watchdogNodeIt->GoToNextItem() )
  {
    vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( watchdogNodeIt->GetCurrentObject() );
    if ( watchdogNode != NULL)
    {
      watchdogNode->InitializeToolbar();
      //QDebug( "Enter: connect toolbar with visibility changed ");
      connect(watchdogNode->WatchdogToolbar, SIGNAL(visibilityChanged(bool)), this, SLOT( onToolbarVisibilityChanged(bool)) );
    }
  }
  watchdogNodeIt->Delete();
  watchdogNodes->Delete();

  this->Superclass::enter();
}

void
qSlicerWatchdogModuleWidget
::setMRMLScene( vtkMRMLScene* scene )
{
  Q_D( qSlicerWatchdogModuleWidget );
  this->Superclass::setMRMLScene( scene );
}

void
qSlicerWatchdogModuleWidget
::onSceneImportedEvent()
{
  this->enter();
  this->updateWidget();
}

void qSlicerWatchdogModuleWidget
::onStatusRefreshRateSpinBoxChanged(int statusRefeshRateMiliSec)
{
  Q_D( qSlicerWatchdogModuleWidget );
  d->logic()->SetStatusRefreshTimeMiliSec(statusRefeshRateMiliSec);
  updateWidget();
}

void qSlicerWatchdogModuleWidget
::onCurrentCellChanged(int currentRow, int currentColumn)
{
  if(CurrentCellPosition[0]!=currentRow && CurrentCellPosition[1]!=currentColumn)
  {
    return;
  }
  Q_D( qSlicerWatchdogModuleWidget );
  vtkMRMLNode* currentNode = d->ModuleNodeComboBox->currentNode();
  if ( currentNode == NULL )
  {
    return;
  }
  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentNode );
  if ( watchdogNode == NULL )
  {
    qCritical( "Selected node not a valid module node" );
    return;
  }
  vtkMRMLDisplayableNode* currentToolNode = vtkMRMLDisplayableNode::SafeDownCast(d->ToolComboBox->currentNode());
  if ( currentToolNode  == NULL )
  {
    return;
  }
  watchdogNode->GetToolNode(currentRow)->label=d->ToolsTableWidget->item(currentRow,currentColumn)->text().toStdString();
  watchdogNode->WatchdogToolbar->SetNodeLabel(currentRow, watchdogNode->GetToolNode(currentRow)->label.c_str());
  disconnect( d->ToolsTableWidget, SIGNAL( cellChanged( int , int ) ), this, SLOT( onCurrentCellChanged( int, int ) ) );
  this->updateWidget();
}

void
qSlicerWatchdogModuleWidget
::onTableItemDoubleClicked()
{
  Q_D( qSlicerWatchdogModuleWidget );
  if(d->ToolsTableWidget->currentColumn()!=0)
  {
    return;
  }
  CurrentCellPosition[0]=d->ToolsTableWidget->currentRow();
  CurrentCellPosition[1]=d->ToolsTableWidget->currentColumn();
  connect( d->ToolsTableWidget, SIGNAL( cellChanged( int , int ) ), this, SLOT( onCurrentCellChanged( int , int ) ) );
}

void
qSlicerWatchdogModuleWidget
::onWatchdogNodeChanged()
{
  Q_D( qSlicerWatchdogModuleWidget );
  this->updateFromMRMLNode();
}

void qSlicerWatchdogModuleWidget
::onWatchdogNodeAddedByUser(vtkMRMLNode* nodeAdded)
{
  Q_D( qSlicerWatchdogModuleWidget );
  //if ( this->mrmlScene() == NULL )
  //{
  //  qCritical() << "Invalid scene!";
  //  return;
  //}
  //// Create a module MRML node if there is none in the scene.
  ////this->topLevelWidget()
  //vtkMRMLNode* node = this->mrmlScene()->GetNthNodeByClass(0, "vtkMRMLWatchdogNode");
  //if ( node == NULL )
  //{
  //  vtkSmartPointer< vtkMRMLWatchdogNode > newNode = vtkSmartPointer< vtkMRMLWatchdogNode >::New();
  //  this->mrmlScene()->AddNode( newNode );
  //}
  //node = this->mrmlScene()->GetNthNodeByClass( 0, "vtkMRMLWatchdogNode" );
  //if ( node == NULL )
  //{
  //  qCritical( "Failed to create module node" );
  //  return;
  //}

  // For convenience, select a default module.
  if(nodeAdded==NULL)
  {
    return;
  }
  vtkMRMLWatchdogNode* watchdogNodeAdded = vtkMRMLWatchdogNode::SafeDownCast( nodeAdded );
  if(watchdogNodeAdded==NULL)
  {
    return;
  }

  watchdogNodeAdded->InitializeToolbar();

  connect(watchdogNodeAdded->WatchdogToolbar, SIGNAL(visibilityChanged(bool)), this, SLOT( onToolbarVisibilityChanged(bool)) );


  this->updateFromMRMLNode();
}

void qSlicerWatchdogModuleWidget
::onModuleNodeAboutToBeRemoved(vtkMRMLNode* nodeToBeRemoved)
{
  Q_D( qSlicerWatchdogModuleWidget );
  if(nodeToBeRemoved==NULL)
  {
    return;
  }

  vtkMRMLWatchdogNode* watchdogNodeToBeRemoved = vtkMRMLWatchdogNode::SafeDownCast( nodeToBeRemoved );
  if(watchdogNodeToBeRemoved==NULL)
  {
    return;
  }

  //QDebug("NODE TO BE REMOVED");
  disconnect(watchdogNodeToBeRemoved->WatchdogToolbar, SIGNAL(visibilityChanged(bool)), this, SLOT( onToolbarVisibilityChanged(bool)) );

  //watchdogNodeToBeRemoved->RemoveToolbar();

  this->updateFromMRMLNode();
  updateWidget();
  d->ToolsTableWidget->clear();
  d->ToolsTableWidget->setRowCount( 0 );
  d->ToolsTableWidget->setColumnCount( 0 );
}

void qSlicerWatchdogModuleWidget
::onToolbarVisibilityButtonClicked()
{
  Q_D( qSlicerWatchdogModuleWidget );

  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( d->ModuleNodeComboBox->currentNode() );
  if ( watchdogNode == NULL )
  {
    qCritical( "Tool node should not be changed when no module node selected" );
    return;
  }

  QMainWindow* window = qSlicerApplication::application()->mainWindow();
  qMRMLWatchdogToolBar *watchdogToolbar =watchdogNode->WatchdogToolbar;

  watchdogToolbar->toggleViewAction()->toggle();
  if(watchdogToolbar->isVisible())
  {
    //d->VisibilityButton->setIcon( QIcon( ":/Icons/Small/SlicerInvisible.png" ) );
    watchdogToolbar->setVisible(false);
  }
  else
  {
    //d->VisibilityButton->setIcon( QIcon( ":/Icons/Small/SlicerVisible.png" ) );
    watchdogToolbar->setVisible(true);
  }
  //watchdogToolbar->visibilityChanged();
  updateWidget();
}

void
qSlicerWatchdogModuleWidget
::updateFromMRMLNode()
{
  Q_D( qSlicerWatchdogModuleWidget );

  vtkMRMLNode* currentNode = d->ModuleNodeComboBox->currentNode();
  if ( currentNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    return;
  }
  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentNode );
  if ( watchdogNode == NULL )
  {
    qCritical( "Selected node not a valid module node" );
    return;
  }

  d->ToolComboBox->setEnabled( true );
  //if ( watchdogNode->GetToolNode() != NULL )
  //{
  //  d->ToolComboBox->setCurrentNodeID( QString::fromStdString( watchdogNode->GetToolNode()->GetID() ) );
  //}
  //else
  //{
  //  d->ToolComboBox->setCurrentNodeID( "" );
  //}
  updateWidget();
}

void qSlicerWatchdogModuleWidget
::onTimeout()
{
  updateTable();
}





//void  qSlicerWatchdogModuleWidget
//::updateToolbars()
//{
//  Q_D( qSlicerWatchdogModuleWidget );
//
//  QList<vtkMRMLNode*> watchdogNodesList = d->ModuleNodeComboBox->nodes();
//  QList<vtkMRMLNode*>::Iterator it;
//  for (it = watchdogNodesList.begin(); it != watchdogNodesList.end(); ++it)
//  {
//    vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( (*it) );
//    if(watchdogNode->WatchdogToolbar && watchdogNode->WatchdogToolbar->isVisible())
//    {
//      std::list<WatchedTool>* toolsVectorPtr = watchdogNode->GetToolNodes();
//      int numberTools = toolsVectorPtr->size();
//      if ( toolsVectorPtr == NULL /*|| numberTools!= d->ToolsTableWidget->rowCount()*/)
//      {
//        return;
//      }
//      int row=0;
//      for (std::list<WatchedTool>::iterator itTool = toolsVectorPtr->begin() ; itTool != toolsVectorPtr->end(); ++itTool)
//      {
//        if((*itTool).tool==NULL)
//        {
//          return;
//        }
//        watchdogNode->WatchdogToolbar->SetNodeStatus(row,(*itTool).status);
//        row++;
//      }
//    }
//  }
//}

void  qSlicerWatchdogModuleWidget
::updateTable()
{
  Q_D( qSlicerWatchdogModuleWidget );
  vtkMRMLNode* currentNode = d->ModuleNodeComboBox->currentNode();
  if ( currentNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    return;
  }
  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentNode );
  if ( watchdogNode == NULL )
  {
    qCritical( "Selected node not a valid module node" );
    return;
  }
  std::list<WatchedTool> *toolsVectorPtr = watchdogNode->GetToolNodes();
  int numberTools= toolsVectorPtr->size();
  if ( toolsVectorPtr == NULL || numberTools != d->ToolsTableWidget->rowCount())
  {
    return;
  }
  int row=0;
  for (std::list<WatchedTool>::iterator it = toolsVectorPtr->begin() ; it != toolsVectorPtr->end(); ++it)
  {
    if((*it).tool==NULL)
    {
      return;
    }

    d->ToolsTableWidget->blockSignals( true );
    //QTableWidgetItem* labelItem = new QTableWidgetItem( (*it).tool->GetName() );
    //d->ToolsTableWidget->setItem( row, 0, labelItem );
    if((*it).status==0)
    {
      //QTableWidgetItem* lastElapsedTimeStatus = new QTableWidgetItem( QString::number( floor(ElapsedTimeSec-(*it).lastElapsedTimeStamp) ) );
      //d->ToolsTableWidget->setItem( row, TOOL_TIMESTAMP_COLUMN, lastElapsedTimeStatus );
      d->ToolsTableWidget->item( row, TOOL_TIMESTAMP_COLUMN)->setBackground(Qt::red);
      QString disconnectedString ("Disconnected ");
      disconnectedString += QString::number( floor(d->logic()->GetElapsedTimeSec()-(*it).lastElapsedTimeStamp) )+ " [s] ";
      d->ToolsTableWidget->item( row, TOOL_TIMESTAMP_COLUMN)->setText(disconnectedString);
    }
    else
    {
      d->ToolsTableWidget->item( row, TOOL_TIMESTAMP_COLUMN)->setText("");
      d->ToolsTableWidget->item( row, TOOL_TIMESTAMP_COLUMN)->setBackground(QBrush(QColor(45,224,90)));
    }
    d->ToolsTableWidget->blockSignals( false );
    row++;
  }
}

void  qSlicerWatchdogModuleWidget
::onDownButtonClicked()
{
  Q_D( qSlicerWatchdogModuleWidget);
  vtkMRMLNode* currentNode = d->ModuleNodeComboBox->currentNode();
  if ( currentNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    return;
  }
  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentNode );
  if ( watchdogNode == NULL )
  {
    return;
  }
  int currentTool = d->ToolsTableWidget->currentRow();
  if ( currentTool < watchdogNode->GetNumberOfTools()- 1 && currentTool>=0 )
  {
    watchdogNode->SwapTools( currentTool, currentTool + 1 );
  }
  updateWidget();
}

void  qSlicerWatchdogModuleWidget
::onUpButtonClicked()
{
  Q_D( qSlicerWatchdogModuleWidget);
  vtkMRMLNode* currentNode = d->ModuleNodeComboBox->currentNode();
  if ( currentNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    return;
  }
  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentNode );
  if ( watchdogNode == NULL )
  {
    return;
  }
  int currentTool = d->ToolsTableWidget->currentRow();
  if ( currentTool > 0 )
  {
    watchdogNode->SwapTools( currentTool, currentTool - 1 );
  }
  updateWidget();
}

void  qSlicerWatchdogModuleWidget
::onDeleteButtonClicked()
{
  Q_D( qSlicerWatchdogModuleWidget);

  //vtkMRMLTransformNode* currentTransformNode = vtkMRMLMarkupsNode::SafeDownCast( d->ToolComboBox->currentNode() );
  //if ( currentTransformNode  == NULL )
  //{
  //  return;
  //}
  QItemSelectionModel* selectionModel = d->ToolsTableWidget->selectionModel();
  std::vector< int > deleteFiducials;
  // Need to find selected before removing because removing automatically refreshes the table
  for ( int i = 0; i < d->ToolsTableWidget->rowCount(); i++ )
  {
    if ( selectionModel->rowIntersectsSelection( i, d->ToolsTableWidget->rootIndex() ) )
    {
      deleteFiducials.push_back( i );
    }
  }
  vtkMRMLNode* currentNode = d->ModuleNodeComboBox->currentNode();
  if ( currentNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    return;
  }
  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentNode );
  if ( watchdogNode == NULL )
  {
    return;
  }
  //Traversing this way should be more efficient and correct
  QString toolKey;
  for ( int i = deleteFiducials.size() - 1; i >= 0; i-- )
  {
    watchdogNode->RemoveTool(deleteFiducials.at( i ));
  }
  this->updateWidget();
}

void qSlicerWatchdogModuleWidget
::onToolNodeAdded( )
{
  Q_D(qSlicerWatchdogModuleWidget);
  vtkMRMLNode* currentNode = d->ModuleNodeComboBox->currentNode();
  if ( currentNode == NULL )
  {
    return;
  }

  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentNode );
  if ( watchdogNode == NULL )
  {
    qCritical( "Selected node not a valid module node" );
    return;
  }

  vtkMRMLDisplayableNode* currentToolNode = vtkMRMLDisplayableNode::SafeDownCast(d->ToolComboBox->currentNode());
  if ( currentToolNode  == NULL )
  {
    return;
  }

  d->logic()->AddToolNode(watchdogNode, currentToolNode ); // Make sure there is an associated display node
  this->updateWidget();
}

void qSlicerWatchdogModuleWidget
::onToolsTableContextMenu(const QPoint& position)
{
  Q_D(qSlicerWatchdogModuleWidget);

  QPoint globalPosition = d->ToolsTableWidget->viewport()->mapToGlobal( position );

  QMenu* trasnformsMenu = new QMenu( d->ToolsTableWidget );
  QAction* activateAction = new QAction( "Make list active", trasnformsMenu );
  QAction* deleteAction = new QAction( "Delete highlighted row", trasnformsMenu );
  QAction* upAction = new QAction( "Move current element up", trasnformsMenu );
  QAction* downAction = new QAction( "Move current row down", trasnformsMenu );
  QAction* jumpAction = new QAction( "Jump slices to row", trasnformsMenu );

  trasnformsMenu->addAction( activateAction );
  trasnformsMenu->addAction( deleteAction );
  trasnformsMenu->addAction( upAction );
  trasnformsMenu->addAction( downAction );
  trasnformsMenu->addAction( jumpAction );

  QAction* selectedAction = trasnformsMenu->exec( globalPosition );

  int currentTool = d->ToolsTableWidget->currentRow();
  //vtkMRMLTransformNode* currentNode = vtkMRMLTransformNode::SafeDownCast( d->ToolComboBox->currentNode() );
  vtkMRMLNode* currentNode = d->ModuleNodeComboBox->currentNode();
  if ( currentNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    return;
  }
  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentNode );

  if ( watchdogNode == NULL )
  {
    return;
  }

  if ( selectedAction == deleteAction )
  {
    QItemSelectionModel* selectionModel = d->ToolsTableWidget->selectionModel();
    std::vector< int > deleteFiducials;
    // Need to find selected before removing because removing automatically refreshes the table
    for ( int i = 0; i < d->ToolsTableWidget->rowCount(); i++ )
    {
      if ( selectionModel->rowIntersectsSelection( i, d->ToolsTableWidget->rootIndex() ) )
      {
        deleteFiducials.push_back( i );
      }
    }
    //Traversing this way should be more efficient and correct
    QString toolKey;
    for ( int i = deleteFiducials.size() - 1; i >= 0; i-- )
    {
      watchdogNode->RemoveTool(deleteFiducials.at( i ));
    }
  }

  if ( selectedAction == upAction )
  {
    if ( currentTool > 0 )
    {
      watchdogNode->SwapTools( currentTool, currentTool - 1 );
    }
  }

  if ( selectedAction == downAction )
  {
    if ( currentTool < watchdogNode->GetNumberOfTools()- 1 )
    {
      watchdogNode->SwapTools( currentTool, currentTool + 1 );
    }
  }
  this->updateWidget();
}

void qSlicerWatchdogModuleWidget
::updateWidget()
{
  Q_D(qSlicerWatchdogModuleWidget);
  vtkMRMLNode* currentModuleNode = d->ModuleNodeComboBox->currentNode();
  if ( currentModuleNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    d->ToolsTableWidget->clear();
    d->ToolsTableWidget->setRowCount( 0 );
    d->ToolsTableWidget->setColumnCount( 0 );
    return;
  }
  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentModuleNode );
  if ( watchdogNode == NULL )
  {
    qCritical( "Selected node not a valid module node" );
    return;
  }
  vtkMRMLDisplayableNode* currentToolNode = vtkMRMLDisplayableNode::SafeDownCast( d->ToolComboBox->currentNode() );
  d->AddToolButton->blockSignals( true );
  if ( currentToolNode == NULL )
  {
    d->AddToolButton->setChecked( Qt::Unchecked );
    // This will ensure that we refresh the widget next time we move to a non-null widget (since there is guaranteed to be a modified status of larger than zero)
    //return;
    d->AddToolButton->setEnabled(false);
  }
  else
  {
    // Set the button indicating if this list is active
    if ( !watchdogNode->HasTool(currentToolNode->GetName()))
    {
      d->AddToolButton->setEnabled(true);
    }
    else
    {
      d->AddToolButton->setEnabled(false);
    }
  }
  d->AddToolButton->blockSignals( false );

  // Update the fiducials table
  d->ToolsTableWidget->blockSignals( true );

for (int row=0; row<d->ToolsTableWidget->rowCount(); row++)
{
  QCheckBox *soundCheckBox = dynamic_cast <QCheckBox *> (d->ToolsTableWidget->cellWidget(row,TOOL_SOUND_COLUMN));
  disconnect(soundCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onSoundCheckBoxStateChanged(int)));
}

  d->ToolsTableWidget->clear();
  QStringList MarkupsTableHeaders;
  MarkupsTableHeaders << "Label" << "Name" << "Sound" << "Status";
  d->ToolsTableWidget->setRowCount( watchdogNode->GetNumberOfTools() );
  d->ToolsTableWidget->setColumnCount( TOOL_COLUMNS );
  d->ToolsTableWidget->setHorizontalHeaderLabels( MarkupsTableHeaders );
  d->ToolsTableWidget->horizontalHeader()->setResizeMode( QHeaderView::Stretch );
  std::string fiducialLabel = "";

  std::list<WatchedTool> *toolsVectorPtr = watchdogNode->GetToolNodes();
  int numberTools= toolsVectorPtr->size();
  if ( toolsVectorPtr == NULL || numberTools != d->ToolsTableWidget->rowCount())
  {
    return;
  }
  int row=0;
  for (std::list<WatchedTool>::iterator it = toolsVectorPtr->begin() ; it != toolsVectorPtr->end(); ++it)
  {
    if((*it).tool==NULL)
    {
      return;
    }
    //d->ToolsTableWidget->blockSignals( true );
    QTableWidgetItem* nameItem = new QTableWidgetItem( (*it).tool->GetName() );
    QTableWidgetItem* labelItem = new QTableWidgetItem( (*it).label.c_str() );
    QTableWidgetItem* lastElapsedTimeStatus = new QTableWidgetItem( "" );
    lastElapsedTimeStatus->setTextAlignment(Qt::AlignCenter);

    QCheckBox *pCheckBox = new QCheckBox();
    pCheckBox->setAccessibleName(QString::number(row));
    if((*it).sound)
    {
      pCheckBox->setCheckState(Qt::Checked);
    }
    else
    {
      pCheckBox->setCheckState(Qt::Unchecked);
    }
    pCheckBox->setStyleSheet("margin-left:50%; margin-right:50%;");
    connect(pCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onSoundCheckBoxStateChanged(int)));

    d->ToolsTableWidget->setItem( row, TOOL_TIMESTAMP_COLUMN, lastElapsedTimeStatus );
    d->ToolsTableWidget->setItem( row, TOOL_NAME_COLUMN, nameItem );
    d->ToolsTableWidget->setItem( row, TOOL_LABEL_COLUMN, labelItem );
    d->ToolsTableWidget->setCellWidget(row,TOOL_SOUND_COLUMN, pCheckBox);

    row++;
  }
  updateTable();
  d->ToolsTableWidget->blockSignals( false );

  if(watchdogNode->WatchdogToolbar && watchdogNode->WatchdogToolbar->isVisible())
  {
    d->ToolbarVisibilityCheckBox->setChecked(true);//d->VisibilityButton->setIcon( QIcon( ":/Icons/Small/SlicerVisible.png" ) );
  }
  else
  {
    d->ToolbarVisibilityCheckBox->setChecked(false);//d->VisibilityButton->setIcon( QIcon( ":/Icons/Small/SlicerInvisible.png" ) );
  }
  //emit updateFinished();
}



void qSlicerWatchdogModuleWidget
::onSoundCheckBoxStateChanged(int state)
{
  Q_D(qSlicerWatchdogModuleWidget);

  vtkMRMLNode* currentNode = d->ModuleNodeComboBox->currentNode();
  if ( currentNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    return;
  }
  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentNode );

  if ( watchdogNode == NULL )
  {
    return;
  }
  QCheckBox *pCheckBox = dynamic_cast <QCheckBox *>(QObject::sender());
  int cbRow = pCheckBox->accessibleName().toInt();
  if(cbRow>=0&&cbRow<watchdogNode->GetNumberOfTools())
  {
    //QDebug("Changed state checkbox current cell position = %d", cbRow);
    watchdogNode->GetToolNode(cbRow)->sound=state;
  }
  //int currentTool = d->ToolsTableWidget->currentRow();
  //QTableWidgetItem *pCheckBox = dynamic_cast <QTableWidgetItem *>(QObject::sender());
  //for(int row =0; row<watchdogNode->GetNumberOfTools();row++)
  //{
  //  QCheckBox *tCheckBox=d->ToolsTableWidget->cellWidget(row,TOOL_SOUND_COLUMN);
  //  if(==pCheckBox)
  //  {
  //    break;
  //    cbRow=pCheckBox->name()).toUInt();
  //  }
  //}
}


void qSlicerWatchdogModuleWidget
::onToolbarVisibilityChanged( bool visible )
{
  Q_D(qSlicerWatchdogModuleWidget);
  vtkMRMLNode* currentModuleNode = d->ModuleNodeComboBox->currentNode();
  if ( currentModuleNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    return;
  }
  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( currentModuleNode );
  if(watchdogNode->WatchdogToolbar && watchdogNode->WatchdogToolbar->isVisible())
  {
    d->ToolbarVisibilityCheckBox->setChecked(true);//d->VisibilityButton->setIcon( QIcon( ":/Icons/Small/SlicerVisible.png" ) );
  }
  else
  {
    d->ToolbarVisibilityCheckBox->setChecked(false);//d->VisibilityButton->setIcon( QIcon( ":/Icons/Small/SlicerInvisible.png" ) );
  }
}

//vtkMRMLNode* qSlicerWatchdogModuleWidget
//::GetCurrentNode()
//{
//  Q_D(qSlicerWatchdogModuleWidget);
//
//  return d->ToolComboBox->currentNode();
//}
//void qSlicerWatchdogModuleWidget
//::SetCurrentNode( vtkMRMLNode* currentNode )
//{
//  Q_D(qSlicerWatchdogModuleWidget);
//  if(currentNode==NULL)
//  {
//    return;
//  }
//
//  vtkMRMLDisplayableNode* currentDisplayableNode = vtkMRMLDisplayableNode::SafeDownCast( currentNode );
//  // Don't change the active fiducial list if the current node is changed programmatically
//  disconnect( d->ToolComboBox, SIGNAL( currentNodeChanged( vtkMRMLNode* ) ), this, SLOT( updateWidget() ) );
//  d->ToolComboBox->setCurrentNode( currentDisplayableNode );
//  connect( d->ToolComboBox, SIGNAL( currentNodeChanged( vtkMRMLNode* ) ), this, SLOT( updateWidget() ) );
//  // Reconnect the appropriate nodes
//  this->qvtkDisconnectAll();
//  //this->ConnectInteractionAndSelectionNodes();
//  this->qvtkConnect( currentDisplayableNode, vtkCommand::ModifiedEvent, this, SLOT( updateWidget() ) );
//  //this->qvtkConnect( currentTransformNode, vtkMRMLTransformNode::MarkupAddedEvent, d->TransformsTableWidget, SLOT( scrollToBottom() ) );
//
//  this->updateWidget(); // Must call this to update widget even if the node hasn't changed - this will cause the active button and table to update
//}

////-----------------------------------------------------------------------------TODO. THIS MIGHT NOT BE NEEDED!!!
//void qSlicerWatchdogModuleWidget::onToolChanged()
//{
//  Q_D( qSlicerWatchdogModuleWidget );
//
//  vtkMRMLWatchdogNode* moduleNode = vtkMRMLWatchdogNode::SafeDownCast( d->ModuleNodeComboBox->currentNode() );
//  if ( moduleNode == NULL )
//  {
//    qCritical( "Tool node should not be changed when no module node selected" );
//    return;
//  }
//
//  vtkMRMLNode* currentNode = d->ToolComboBox->currentNode();
//  if ( currentNode == NULL )
//  {
//    return;
//    //d->logic()->SetObservedToolNode( NULL, moduleNode );
//  }
//
//  //default refresh rate 4 [Hz]
//  //this->Timer->start( 250 );
//  updateWidget();
//}

//void qSlicerWatchdogModuleWidget
//::onTransformsEdited( int row, int column )
//{
//  Q_D(qSlicerWatchdogModuleWidget);
//
//  vtkMRMLTransformNode* currentTransformNode = vtkMRMLTransformNode::SafeDownCast( this->GetCurrentNode() );
//
//  if ( currentTransformNode == NULL )
//  {
//    return;
//  }
//
//  // Find the fiducial's current properties
//  double currentFiducialPosition[3] = { 0, 0, 0 };
//  currentTransformNode->GetNthFiducialPosition( row, currentFiducialPosition );
//  std::string currentFiducialLabel = currentTransformNode->GetNthFiducialLabel( row );
//
//  // Find the entry that we changed
//  QTableWidgetItem* qItem = d->TransformsTableWidget->item( row, column );
//  QString qText = qItem->text();
//
//  if ( column == FIDUCIAL_LABEL_COLUMN )
//  {
//    currentTransformNode->SetNthFiducialLabel( row, qText.toStdString() );
//  }
//
//  //// Check if the value can be converted to double is already performed implicitly
//  //double newFiducialPosition = qText.toDouble();
//  //// Change the position values
//  //if ( column == FIDUCIAL_X_COLUMN )
//  //{
//  //  currentFiducialPosition[ 0 ] = newFiducialPosition;
//  //}
//  //if ( column == FIDUCIAL_Y_COLUMN )
//  //{
//  //  currentFiducialPosition[ 1 ] = newFiducialPosition;
//  //}
//  //if ( column == FIDUCIAL_Z_COLUMN )
//  //{
//  //  currentFiducialPosition[ 2 ] = newFiducialPosition;
//  //}
//  //currentTransformNode->SetNthFiducialPositionFromArray( row, currentFiducialPosition );
//
//  this->updateWidget(); // This may not be necessary the widget is updated whenever a fiducial is changed
//}





