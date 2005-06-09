function [Alist,b] = deleteUnderlyingData(grid,k,q,Alist,b)
%DELETEUNDERLYINGDATA  Delete parent data underlying the current patch.
%   [Alist,b] = deleteUnderlyingData(grid,k,q) deletes the discrete
%   equations of the parent patch of patch q at level k, and replaces them
%   with the identity operator. Note that equations at parent patch outside
%   patch-q-at-level-k at also affected. We replace the parent patch matrix
%   by the identity matrix with zero RHS to make the underlying data = 0.
%
%   See also: TESTDFM, SETOPERATOR, SETOPERATORPATCH, ADDPATCH.

fprintf('--- deleteUnderlyingData(k = %d, q = %d) ---\n',k,q);
level       = grid.level{k};
numPatches  = length(level.numPatches);
s           = level.stencilOffsets;
numEntries  = size(s,1);
h           = level.h;
P           = grid.level{k}.patch{q};
map         = P.cellIndex;

if (P.parent < 0)                                   % Base patch at coarsest level, nothing to delete
    fprintf('Nothing to delete\n');
    return;
end

% Find box in Q-coordinates underlying P
Q           = grid.level{k-1}.patch{P.parent};      % Parent patch
underLower = coarsenIndex(grid,k,P.ilower);
underUpper = coarsenIndex(grid,k,P.iupper);
under       = cell(grid.dim,1);
for dim = 1:grid.dim
    under{dim} = [underLower(dim)-1:underUpper(dim)+1] + Q.offset(dim);  % Patch-based cell indices including ghosts
end
matUnder      = cell(grid.dim,1);
[matUnder{:}] = ndgrid(under{:});
pindexUnder   = sub2ind(P.size,matUnder{:});                      % Patch-based cell indices - list
pindexUnder   = pindexUnder(:);
mapUnder      = Q.cellIndex(pindexUnder);

% Compute chunk of patch Q (level k-1) equations of the underlying area and
% subtract them from the equations so that they disappear from level k-1 equations.
[AlistUnder,bUnder] = setupOperatorPatch(grid,k-1,P.parent,underLower,underUpper);
AlistUnder(:,3) = -AlistUnder(:,3);
Alist = [Alist; AlistUnder];
b(mapUnder(:)) = bUnder(mapUnder - Q.baseIndex + 1);

% Place an identity operator over the deleted region and ghost cells of it
% that are at domain boundaries (B.C. are now defined at patch P instead).
[AlistUnder,bUnder] = setupIdentityPatch(grid,k-1,P.parent,underLower,underUpper);
Alist = [Alist; AlistUnder];
b(mapUnder(:)) = bUnder(mapUnder - Q.baseIndex + 1);
